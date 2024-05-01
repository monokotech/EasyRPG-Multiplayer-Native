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
#include "uv.h"

/**
 * Socket
 */

class Socket {
public:
	constexpr static size_t BUFFER_SIZE = 4096;
	constexpr static size_t HEAD_SIZE = sizeof(uint16_t);

	Socket();

	std::function<void(const char*, const size_t&)> OnData;
	std::function<void()> OnOpen;
	std::function<void()> OnClose;

	void InitStream(uv_loop_t* loop) {
		stream.data = this;
		uv_tcp_init(loop, &stream);
	}
	uv_tcp_t* GetStream() {
		return &stream;
	}

	void Send(std::string_view& data);

	void Open();
	void Close();

private:
	int err;

	uv_tcp_t stream;
	uv_write_t send_req;

	// use queue: buffers must remain valid while sending
	std::queue<std::unique_ptr<std::vector<char>>> m_send_queue;
	bool is_sending = false;

	void ContinueSend();

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
	int err;

	struct AsyncData {
		uv_loop_t* loop;
		bool stop_flag;
	} async_data;
	uv_async_t async;

	std::string addr_host;
	uint16_t addr_port;

	std::string socks5_req_addr_host;
	uint16_t socks5_req_addr_port;

	bool manually_close_flag = false;
	bool is_connected = false;

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
		uv_loop_t* loop;
		bool stop_flag;
	} async_data;
	uv_async_t async;

	std::string addr_host;
	uint16_t addr_port;

	bool is_running = false;

public:
	ServerListener(const std::string _host, const uint16_t _port)
		: addr_host(_host), addr_port(_port) {}

	void Start(bool wait_thread = false);
	void Stop();

	std::function<void(std::unique_ptr<Socket>)> OnConnection;
};

#endif
