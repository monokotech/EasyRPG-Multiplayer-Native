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
#include <cstring>
#include <memory>
#include <queue>
#include <mutex>
#include "uv.h"

/**
 * Socket
 */

class Socket {
public:
	constexpr static size_t BUFFER_SIZE = 4096;
	constexpr static size_t HEAD_SIZE = sizeof(uint16_t);

	Socket();

	enum class AsyncCall {
		SEND,
		OPEN,
		CLOSE,
	};

	std::function<void()> OnOpen;
	std::function<void()> OnClose;
	std::function<void(std::string_view data)> OnData;

	void InitStream(uv_loop_t* loop);

	uv_tcp_t* GetStream() {
		return &stream;
	}

	void Send(std::string_view data);
	void Open();
	void Close();

private:
	void InternalOnData(const char* buf, const ssize_t num_bytes) {
		std::string_view data(reinterpret_cast<const char*>(buf), num_bytes);
		OnData(data);
	}

	std::mutex m_call_mutex;
	std::queue<AsyncCall> m_request_queue;

	struct AsyncData {
		Socket *socket;
	} async_data;
	uv_async_t async;

	uv_tcp_t stream;
	uv_write_t send_req;

	// use queue: buffers must remain valid while sending
	std::queue<std::unique_ptr<std::vector<char>>> m_send_queue;
	bool is_sending = false;

	bool is_initialized = false;

	void InternalOpen();
	void InternalClose();
	void InternalSend();

	struct StreamRead {
		Socket *socket;

		bool got_head = false;
		uint16_t data_size = 0;
		uint16_t begin = 0;
		char tmp_buf[BUFFER_SIZE];
		uint16_t tmp_buf_used = 0;

		void Handle(char *buf, ssize_t buf_used);
	} stream_read;
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

	std::string socks5_req_addr_host;
	uint16_t socks5_req_addr_port;

	bool manually_close_flag;
	bool is_connect = false;

public:
	std::function<void()> OnConnect;
	std::function<void()> OnDisconnect;

	void Connect(const std::string_view _host, const uint16_t _port);
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
};

#endif
