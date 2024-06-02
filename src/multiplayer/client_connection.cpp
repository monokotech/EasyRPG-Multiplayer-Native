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
#include "output_mt.h"

#ifndef EMSCRIPTEN
#  include "socket.h"
#else
#  include <emscripten/websocket.h>
#endif

#ifdef EMSCRIPTEN
class ConnectorSocket {
public:
	void SetReadTimeout(uint16_t _read_timeout_ms) { read_timeout_ms = _read_timeout_ms; }

	void ConfigSocks5(std::string_view host, const uint16_t port) {
		if (!host.empty()) {
			OnWarning(std::string("EM_WS: Enable SOCKS5 from the browser. addr: ")
					.append(host) + ":" + std::to_string(port));
		}
	}

	void SetRemoteAddress(std::string_view host, uint16_t port) {
		std::string_view ws_protocol = "ws://";
		std::string_view path = "";
		if (host.empty()) {
			port = 80;
		}

		// WebSocket protocol
		std::string_view location_protocol = reinterpret_cast<const char*>(EM_ASM_PTR({
			return stringToNewUTF8(window.location.protocol);
		}));
		if (location_protocol == "https:") {
			ws_protocol = "wss://";
			if (host.empty()) {
				port = 443;
			}
		}

		if (host.empty()) {
			// Hostname
			std::string_view location_hostname = reinterpret_cast<const char*>(EM_ASM_PTR({
				return stringToNewUTF8(window.location.hostname);
			}));
			host = location_hostname;

			// Appended path (Use reverse proxy for ports 80 or 443)
			path = "/connect";
		}

		// Final outcome: ws_protocol://host:port/path
		url = std::string(ws_protocol).append(host) + ":" + std::to_string(port).append(path);
	}

	std::function<void(std::string_view data)> OnMessage;

	void Send(std::string_view data) {
		emscripten_websocket_send_binary(websocket, (void*)data.data(), data.size());
	}

	std::function<void()> OnConnect;
	std::function<void()> OnDisconnect;
	std::function<void()> OnFail;

	void Connect() {
		EmscriptenWebSocketCreateAttributes ws_attrs = {
			url.c_str(),
			"binary",
			EM_TRUE,
		};
		websocket = emscripten_websocket_new(&ws_attrs);

		/* Set websocket callbacks */
		emscripten_websocket_set_onmessage_callback(websocket, this,
				[](int eventType, const EmscriptenWebSocketMessageEvent *event, void *userData) -> EM_BOOL {
			auto socket = static_cast<ConnectorSocket*>(userData);
			socket->SetReadTimeout();
			socket->OnMessage(std::string_view(reinterpret_cast<const char*>(event->data), event->numBytes));
			return EM_TRUE;
		});
		emscripten_websocket_set_onopen_callback(websocket, this,
				[](int eventType, const EmscriptenWebSocketOpenEvent *event, void *userData) -> EM_BOOL {
			auto socket = static_cast<ConnectorSocket*>(userData);
			socket->SetReadTimeout();
			socket->OnConnect();
			socket->OnInfo(std::string("EM_WS: Created a connection from: ").append(socket->url));
			return EM_TRUE;
		});
		emscripten_websocket_set_onclose_callback(websocket, this,
				[](int eventType, const EmscriptenWebSocketCloseEvent *event, void *userData) -> EM_BOOL {
			auto socket = static_cast<ConnectorSocket*>(userData);
			socket->ClearReadTimeout();
			if (event->code > 1000) {
				std::string_view reason = {event->reason};
				if (reason.empty())
					socket->OnWarning(std::string("EM_WS: Status code: ").append(std::to_string(event->code)));
				else
					socket->OnWarning(std::string("EM_WS: ")
							.append("(code " + std::to_string(event->code) + ") ").append(reason));
				switch (event->code) {
				case 1006:
					socket->OnDisconnect();
					break;
				default:
					socket->OnFail();
				};
			} else {
				socket->OnDisconnect();
			}
			socket->OnInfo(std::string("EM_WS: Connection closed from: ").append(socket->url));
			return EM_TRUE;
		});
	}

	void Disconnect() {
		ClearReadTimeout();
		// Calling `close` will not fire the `onclose` event
		emscripten_websocket_close(websocket, 1000, nullptr);
		emscripten_websocket_delete(websocket);
		OnInfo("EM_WS: Disconnected");
	}

	std::function<void(std::string_view data)> OnInfo;
	std::function<void(std::string_view data)> OnWarning;

private:
	std::string url;

	EMSCRIPTEN_WEBSOCKET_T websocket;

	/**
	 * Read Timeout interval
	 */

	uint16_t read_timeout_ms = 0;
	int read_timeout_id = 0;

	void ClearReadTimeout() {
		if (read_timeout_id) {
			emscripten_clear_interval(read_timeout_id);
			read_timeout_id = 0;
		}
	}

	void SetReadTimeout() {
		if (!read_timeout_ms) {
			return;
		}
		ClearReadTimeout();
		read_timeout_id = emscripten_set_interval([](void *userData) {
			auto socket = static_cast<ConnectorSocket*>(userData);
			socket->OnWarning(std::string("EM_WS: Read timeout: ").append(socket->url));
			socket->Disconnect();
			socket->OnDisconnect();
		}, read_timeout_ms, this);
	}
};
#endif

static constexpr size_t QUEUE_MAX_BULK_SIZE = 4096;

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
			OutputMt::WarningStr("Server exited");
		else if (code == "1")
			OutputMt::WarningStr("Access denied. Too many users");
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
	socket->OnInfo = [](std::string_view m) { OutputMt::Info("{}", m); };
	socket->OnWarning = [](std::string_view m) { OutputMt::Warning("{}", m); };
	socket->SetReadTimeout(cfg->no_heartbeats.Get() ? 0 : 6000);
	socket->SetRemoteAddress(addr_host, addr_port);
	socket->ConfigSocks5(socks5_addr_host, socks5_addr_port);
	socket->OnMessage = [this](auto p1) { HandleData(p1); };
	socket->OnConnect = [this]() { HandleOpen(); };
	socket->OnDisconnect = [this]() { HandleCloseOrTerm(); };
	socket->OnFail = [this]() { HandleCloseOrTerm(true); };
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
	Print("Client Sent: ", data);
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
