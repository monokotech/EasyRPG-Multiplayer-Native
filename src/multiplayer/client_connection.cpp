#include <thread>
#include "client_connection.h"
#include "socket.h"
#include "../output.h"

constexpr size_t MAX_BULK_SIZE = Connection::MAX_QUEUE_SIZE -
		Packet::MSG_DELIM.size();

ClientConnection::ClientConnection() {
	socket.reset(new Socket());
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

void ClientConnection::HandleData(const char* data, const size_t& num_bytes) {
	std::string_view _data(reinterpret_cast<const char*>(data), num_bytes);
	std::lock_guard lock(m_receive_mutex);
	if (_data.size() == 4 && _data.substr(0, 3) == "\uFFFD") {
		std::string_view code = _data.substr(3, 1);
		if (code == "0")
			m_system_queue.push(SystemMessage::EXIT);
		else if (code == "1")
			m_system_queue.push(SystemMessage::ACCESSDENIED_TOO_MANY_USERS);
		return;
	}
	m_data_queue.push(std::move(std::string(_data)));
}

void ClientConnection::Open() {
	if (connected || connecting)
		return;
	socket->OnData = [this](auto p1, auto& p2) { HandleData(p1, p2); };
	socket->OnOpen = [this]() { HandleOpen(); };
	socket->OnClose = [this]() { HandleClose(); };
	socket->Connect(addr_host, addr_port);
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
			auto& e = m_queue.front();
			if (namecmp(e->GetName(), include))
				break;
			auto data = e->ToBytes();
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
