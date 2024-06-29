/*
 * EPMP
 * Copyright (C) 2023 Monokotech
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

#ifndef EP_SOCKET_H
#define EP_SOCKET_H

#include <functional>
#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include "websocket.h"
#include "uv.h"

/**
 * DataHandler
 * Send/OnMessage: Complete message
 */

class DataHandler {
public:
	constexpr static size_t BUFFER_SIZE = 4096;
	constexpr static size_t HEAD_SIZE = sizeof(uint16_t);

	DataHandler();

	void Send(std::string_view data);
	std::function<void(std::string_view data)> OnWrite;

	void GotDataBuffer(const char* buf, size_t buf_used);
	std::function<void(std::string_view data)> OnMessage;

	void Close();
	std::function<void()> OnClose;

	std::function<void(std::string_view data)> OnWarning;

private:
	void OnMessageBuffer(const char* buf, const size_t size) {
		std::string_view data(reinterpret_cast<const char*>(buf), size);
		OnMessage(data);
	}

	bool got_head = false;
	uint16_t data_size = 0;
	uint16_t begin = 0;
	char tmp_buf[BUFFER_SIZE];
	uint16_t tmp_buf_used = 0;

	bool is_protocol_confirmed = false;
	bool is_websocket = false;
	WebSocket websocket;
};

/**
 * Socket
 * Write/OnData: Raw data
 */

class Socket {
public:
	Socket();

	enum class AsyncCall {
		WRITE,
		OPENSOCKET,
		CLOSESOCKET,
	};

	std::function<void(std::string_view data)> OnData;
	std::function<void(std::string_view data)> OnMessage;
	std::function<void()> OnOpen;
	std::function<void()> OnClose;

	void InitStream(uv_loop_t* loop);

	uv_tcp_t* GetStream() {
		return &stream;
	}

	void AsyncKeepAlive(std::unique_ptr<Socket>& socket) {
		async_data.socket_alt_ptr = std::move(socket);
	}

	void SetReadTimeout(uint16_t _read_timeout_ms) {
		read_timeout_ms = _read_timeout_ms;
	}

	inline void Send(std::string_view data) { data_handler.Send(data); }
	void Write(std::string_view data);
	void Open();
	inline void Close() { data_handler.Close(); }
	void CloseSocket();

	std::function<void(std::string_view data)> OnInfo;
	std::function<void(std::string_view data)> OnWarning;

private:
	std::mutex m_call_mutex;
	std::queue<AsyncCall> m_request_queue;

	struct AsyncData {
		Socket* socket;
		// Only to prevent the pointer from being deleted
		std::unique_ptr<Socket> socket_alt_ptr;
	} async_data;
	uv_async_t async;

	uv_tcp_t stream;
	uv_write_t send_req;
	uv_timer_t read_timeout_req;

	uint64_t read_timeout_ms = 0;

	// use queue: buffers must remain valid while sending
	std::queue<std::string> m_send_queue;
	bool is_sending = false;

	bool is_initialized = false;

	void InternalOpenSocket();
	void InternalCloseSocket();
	void InternalWrite();

	DataHandler data_handler;
};

/**
 * ConnectorSocket
 */

class ConnectorSocket : public Socket {
	struct AsyncData {
		bool stop_flag;
	} async_data;
	uv_async_t async;

	std::string addr_host;
	uint16_t addr_port;

	enum class SOCKS5_STEP : std::uint8_t {
		SS_GREETING = 1,
		SS_CONNECTIONREQUEST,
	};
	SOCKS5_STEP socks5_step;
	std::string socks5_req_addr_host;
	uint16_t socks5_req_addr_port;

	bool manually_close_flag;
	bool is_connect = false;
	bool is_failed = false;

public:
	std::function<void()> OnConnect;
	std::function<void()> OnDisconnect;
	std::function<void()> OnFail;

	void SetRemoteAddress(std::string_view host, const uint16_t port);

	// Must call after SetRemoteAddress
	void ConfigSocks5(std::string_view host, const uint16_t port);

	void Connect();
	void Disconnect();
};

/**
 * ServerListener
 */

class ServerListener {
	uv_loop_t loop;

	struct AsyncData {
		bool stop_flag;
	} async_data;
	uv_async_t async;

	std::string addr_host;
	uint16_t addr_port;

	bool is_running = false;

public:
	ServerListener(std::string_view _host, const uint16_t _port)
		: addr_host(_host), addr_port(_port) {}

	void Start(bool wait_thread = false);
	void Stop();

	std::function<void(std::unique_ptr<Socket>)> OnConnection;

	std::function<void(std::string_view data)> OnInfo;
	std::function<void(std::string_view data)> OnWarning;
};

#endif
