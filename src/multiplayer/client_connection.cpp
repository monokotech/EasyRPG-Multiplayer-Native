#include <thread>
#include "client_connection.h"
#include "socket.h"
#include "../output.h"

constexpr size_t MAX_BULK_SIZE = Connection::MAX_QUEUE_SIZE -
		Packet::MSG_DELIM.size();

ClientConnection::ClientConnection() {
	socket.reset(new ConnectorSocket());
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

void ClientConnection::HandleClose() {
	connecting = false;
	connected = false;
	std::lock_guard lock(m_receive_mutex);
	m_system_queue.push(SystemMessage::CLOSE);
}

void ClientConnection::HandleData(std::string_view data) {
	std::lock_guard lock(m_receive_mutex);
	if (data.size() == 4 && data.substr(0, 3) == "\uFFFD") {
		std::string_view code = data.substr(3, 1);
		if (code == "0")
			m_system_queue.push(SystemMessage::EXIT);
		else if (code == "1")
			m_system_queue.push(SystemMessage::ACCESSDENIED_TOO_MANY_USERS);
		return;
	}
	m_data_queue.push(std::move(std::string(data)));
}

void ClientConnection::Open() {
	if (connected || connecting)
		return;
	socket->OnInfo = [](std::string_view m) { Output::Info(m); };
	socket->OnWarning = [](std::string_view m) { Output::Warning(m); };
	socket->SetReadTimeout(cfg->no_heartbeats.Get() ? 0 : 6000);
	socket->SetRemoteAddress(addr_host, addr_port);
	socket->ConfigSocks5(socks5_addr_host, socks5_addr_port);
	socket->OnData = [this](auto p1) { HandleData(p1); };
	socket->OnConnect = [this]() { HandleOpen(); };
	socket->OnDisconnect = [this]() { HandleClose(); };
	socket->Connect();
	connecting = true;
}

void ClientConnection::Close() {
	if (!connected && !connecting)
		return;
	socket->Disconnect();
	m_queue = decltype(m_queue){};
	connecting = false;
	connected = false;
}

void ClientConnection::Send(std::string_view data) {
	if (!connected)
		return;
	socket->Send(data);
}

void ClientConnection::Receive() {
	std::lock_guard lock(m_receive_mutex);
	while (!m_system_queue.empty()) {
		DispatchSystem(m_system_queue.front());
		m_system_queue.pop();
	}
	while (!m_data_queue.empty()) {
		DispatchMessages(m_data_queue.front());
		m_data_queue.pop();
	}
}

/**
 * Here are the four different states of (v != "room") == include:
 *  1. true false: Collect other_pkt.
 *  2. false false: Convert from other_pkt to room_pkt and drain other_pkt(s).
 *  3. false true: Collect room_pkt.
 *  4. true true: Convert from room_pkt to other_pkt and drain room_pkt(s).
 * In simple terms, room_pkt and other_pkt are collected separately for sending, without mixing them.
 */
void ClientConnection::FlushQueue() {
	auto namecmp = [] (std::string_view v, bool include) {
		return (v != "room") == include;
	};

	bool include = false;
	while (!m_queue.empty()) {
		std::string bulk;
		while (!m_queue.empty()) {
			const auto& e = m_queue.front();
			if (namecmp(e->GetName(), include))
				break;
			std::string data = std::move(e->ToBytes());
			// prevent overflow
			if (bulk.size() + data.size() > MAX_BULK_SIZE) {
				Send(bulk);
				bulk.clear();
			}
			if (!bulk.empty())
				bulk += Packet::MSG_DELIM;
			bulk += data;
			m_queue.pop();
		}
		if (!bulk.empty())
			Send(bulk);
		include = !include;
	}
}
