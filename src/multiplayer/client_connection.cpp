/*
 * EPMP
 * See: docs/LICENSE-EPMP.txt
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <thread>
#include "client_connection.h"
#include "messages.h"
#include "../output.h"

#ifndef EMSCRIPTEN
#  include "socket.h"
#endif

static constexpr size_t QUEUE_MAX_BULK_SIZE = 4096;

ClientConnection::ClientConnection() {
#ifndef EMSCRIPTEN
	socket.reset(new ConnectorSocket());
#endif
}
// ->> unused code
ClientConnection::ClientConnection(ClientConnection&& o)
	: Connection(std::move(o)) {}
ClientConnection& ClientConnection::operator=(ClientConnection&& o) {
	Connection::operator=(std::move(o));
	if (this != &o) {
		Close();
	}
	return *this;
}
// <<-
ClientConnection::~ClientConnection() {
	Close();
}

void ClientConnection::SetAddress(std::string_view address) {
	ParseAddress(address.data(), addr_host, addr_port);
}

void ClientConnection::SetSocks5Address(std::string_view address) {
	ParseAddress(address.data(), socks5_addr_host, socks5_addr_port);
}

void ClientConnection::HandleOpen() {
	connecting = false;
	connected = true;
	std::lock_guard lock(m_receive_mutex);
	m_system_queue.push(SystemMessage::OPEN);
}

void ClientConnection::HandleCloseOrTerm(bool terminated) {
	connecting = false;
	connected = false;
	std::lock_guard lock(m_receive_mutex);
	m_system_queue.push(terminated ?
		SystemMessage::TERMINATED : SystemMessage::CLOSE);
}

void ClientConnection::HandleData(std::string_view data) {
	std::lock_guard lock(m_receive_mutex);
	Print("Client Received: ", data);
	if (data.size() == 4 && data.substr(0, 3) == "\uFFFD") {
		std::string_view code = data.substr(3, 1);
		if (code == "0")
			Output::Warning("Server exited");
		else if (code == "1")
			Output::Warning("Access denied. Too many users");
		// Prevent callback to OnDisconnect,
		//  ensure m_system_queue is empty for next connection
		Close();
		m_system_queue.push(SystemMessage::TERMINATED);
		return;
	}
	m_data_queue.push(std::string(data));
}

void ClientConnection::Open() {
	if (connected || connecting)
		return;
#ifndef EMSCRIPTEN
	socket->OnInfo = [](std::string_view m) { Output::Info(m); };
	socket->OnWarning = [](std::string_view m) { Output::Warning(m); };
	socket->SetReadTimeout(cfg->no_heartbeats.Get() ? 0 : 6000);
	socket->SetRemoteAddress(addr_host, addr_port);
	socket->ConfigSocks5(socks5_addr_host, socks5_addr_port);
	socket->OnData = [this](auto p1) { HandleData(p1); };
	socket->OnConnect = [this]() { HandleOpen(); };
	socket->OnDisconnect = [this]() { HandleCloseOrTerm(); };
	socket->OnFail = [this]() { HandleCloseOrTerm(true); };
	socket->Connect();
#endif
	connecting = true;
}

void ClientConnection::Close() {
	if (!connected && !connecting)
		return;
#ifndef EMSCRIPTEN
	socket->Disconnect();
#endif
	m_queue = decltype(m_queue){};
	connecting = false;
	connected = false;
}

void ClientConnection::Send(std::string_view data) {
	if (!connected)
		return;
#ifndef EMSCRIPTEN
	socket->Send(data);
	Print("Client Sent: ", data);
#endif
}

void ClientConnection::Receive() {
	std::lock_guard lock(m_receive_mutex);
	while (!m_system_queue.empty()) {
		DispatchSystem(m_system_queue.front());
		m_system_queue.pop();
	}
	while (!m_data_queue.empty()) {
		Dispatch(m_data_queue.front());
		m_data_queue.pop();
	}
}

/**
 * Here are the four different states of (e->GetType() != RoomPacket::packet_type) == include:
 *  1. true false: Collect other_pkt.
 *  2. false false: Convert from other_pkt to room_pkt and drain other_pkt(s).
 *  3. false true: Collect room_pkt.
 *  4. true true: Convert from room_pkt to other_pkt and drain room_pkt(s).
 * In simple terms, room_pkt and other_pkt are collected separately for sending, without mixing them.
 */
void ClientConnection::FlushQueue() {
	bool include = false;
	while (!m_queue.empty()) {
		std::string bulk;
		while (!m_queue.empty()) {
			const auto& e = m_queue.front();
			if ((e->GetType() != Messages::RoomPacket::packet_type) == include)
				break;
			std::string data = e->ToBytes();
			if (bulk.size() + data.size() > QUEUE_MAX_BULK_SIZE) {
				Send(bulk);
				bulk.clear();
			}
			bulk += data;
			m_queue.pop();
		}
		if (!bulk.empty())
			Send(bulk);
		include = !include;
	}
}
