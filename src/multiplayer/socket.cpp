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

#include <thread>
#include <vector>
#include "socket.h"
#include "connection.h"

// node-main: inspector_socket_server.cc: InspectorSocketServer::Start
// minetest: address.cpp Address::Resolve
// https://stackoverflow.com/questions/25615340/closing-libuv-handles-correctly/25831688#25831688

int Resolve(const std::string& address, const uint16_t port,
		uv_loop_t* loop, struct sockaddr_storage* addr) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AF_UNSPEC;
	uv_getaddrinfo_t req;
	int err = uv_getaddrinfo(loop, &req, nullptr, address.c_str(),
			std::to_string(port).c_str(), &hints);
	if (req.addrinfo->ai_family == AF_INET)
		std::memcpy(addr, req.addrinfo->ai_addr, sizeof(struct sockaddr_in));
	else if (req.addrinfo->ai_family == AF_INET6)
		std::memcpy(addr, req.addrinfo->ai_addr, sizeof(struct sockaddr_in6));
	uv_freeaddrinfo(req.addrinfo);
	return err;
}

/**
 * Socket
 */

Socket::Socket() {
	stream_read.socket = this;
	send_req.data = this;
}

void Socket::InitStream(uv_loop_t* loop) {
	stream.data = this;
	async_data.socket = this;
	async.data = &async_data;
	uv_async_init(loop, &async, [](uv_async_t *handle) {
		auto async_data = static_cast<AsyncData*>(handle->data);
		auto socket = async_data->socket;
		std::lock_guard lock(socket->m_call_mutex);
		while (!socket->m_request_queue.empty()) {
			switch (socket->m_request_queue.front()) {
			case AsyncCall::SEND:
				if (!socket->is_sending &&
						!socket->m_send_queue.empty()) {
					socket->is_sending = true;
					socket->InternalSend();
				}
				break;
			case AsyncCall::OPEN:
				socket->InternalOpen();
				break;
			case AsyncCall::CLOSE:
				socket->InternalClose();
				break;
			}
			socket->m_request_queue.pop();
		}
	});
	uv_tcp_init(loop, &stream);
	is_initialized = true;
}

void Socket::Send(std::string_view& data) {
	std::lock_guard lock(m_call_mutex);

	if (!is_initialized || m_send_queue.size() > 100)
		return;

	const uint16_t data_size = data.size();
	const uint16_t final_size = HEAD_SIZE+data_size;
	auto buf = new std::vector<char>(final_size);
	std::memcpy(buf->data(), &data_size, HEAD_SIZE);
	std::memcpy(buf->data()+HEAD_SIZE, data.data(), data_size);
	m_send_queue.emplace(buf);

	m_request_queue.push(AsyncCall::SEND);
	uv_async_send(&async);
}

void Socket::InternalSend() {
	auto& vec_buf = m_send_queue.front();
	uv_buf_t buf = uv_buf_init(vec_buf->data(), vec_buf->size());
	uv_write(&send_req, reinterpret_cast<uv_stream_t*>(&stream), &buf, 1,
			[](uv_write_t* req, int err) {
		auto socket = static_cast<Socket*>(req->data);
		if (err) {
			Output::Debug("Error writing to the stream: {}", uv_strerror(err));
		}
		if (socket->is_sending) {
			socket->m_send_queue.pop();
			if (socket->m_send_queue.empty()) {
				socket->is_sending = false;
			} else {
				socket->InternalSend();
			}
		}
	});
}

void Socket::StreamRead::Handle(char *buf, ssize_t buf_used) {
	/**
	 * Resolve the issue of TCP packet sticking or unpacking
	 * Read/Write: Header (2 bytes) + Data (Maximum 64KiB)
	 */
	while (begin < buf_used) {
		uint16_t buf_remaining = buf_used-begin;
		uint16_t tmp_buf_remaining = BUFFER_SIZE-tmp_buf_used;
		if (tmp_buf_used > 0) {
			if (got_head) {
				uint16_t data_remaining = data_size-tmp_buf_used;
				// There is enough temporary space to write into
				if (data_remaining <= tmp_buf_remaining) {
					if (data_remaining <= buf_remaining) {
						std::memcpy(tmp_buf+tmp_buf_used, buf+begin, data_remaining);
						socket->OnData(tmp_buf, data_size);
						begin += data_remaining;
					} else {
						if (buf_remaining > 0) {
							std::memcpy(tmp_buf+tmp_buf_used, buf+begin, buf_remaining);
							tmp_buf_used += buf_remaining;
						}
						break; // Go to the next packet
					}
				} else {
					//OnLogDebug(LABEL + ": Exception (data): "
					//	+ std::string("tmp_buf_used: ") + std::to_string(tmp_buf_used)
					//	+ std::string(", data_size: ") + std::to_string(data_size)
					//	+ std::string(", data_remaining: ") + std::to_string(data_remaining));
				}
				got_head = false;
				tmp_buf_used = 0;
				data_size = 0;
			} else {
				uint16_t head_remaining = HEAD_SIZE-tmp_buf_used;
				if (head_remaining <= buf_remaining && head_remaining <= tmp_buf_remaining) {
					std::memcpy(tmp_buf+tmp_buf_used, buf+begin, head_remaining);
					std::memcpy(&data_size, tmp_buf, HEAD_SIZE);
					begin += head_remaining;
					got_head = true;
				}
				tmp_buf_used = 0;
			}
		} else {
			// The header can be taken out from buf_remaining
			if (!got_head && HEAD_SIZE <= buf_remaining) {
				std::memcpy(&data_size, buf+begin, HEAD_SIZE);
				begin += HEAD_SIZE;
				got_head = true;
			// Data can be taken out from buf_remaining
			} else if (got_head && data_size <= buf_remaining) {
				socket->OnData(buf+begin, data_size);
				begin += data_size;
				got_head = false;
				data_size = 0;
			} else if (buf_remaining < HEAD_SIZE || buf_remaining < data_size) {
				if (buf_remaining > 0 && buf_remaining <= tmp_buf_remaining) {
					std::memcpy(tmp_buf+tmp_buf_used, buf+begin, buf_remaining);
					tmp_buf_used += buf_remaining;
				}
				break; // Go to the next packet
			}
		}
		// Ignore empty data
		if (got_head && data_size == 0) {
			got_head = false;
		}
	}
	begin = 0;
}

void Socket::Open() {
	std::lock_guard lock(m_call_mutex);
	m_request_queue.push(AsyncCall::OPEN);
	uv_async_send(&async);
}

void Socket::InternalOpen() {
	uv_read_start(reinterpret_cast<uv_stream_t*>(&stream),
			[](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
		buf->base = new char[BUFFER_SIZE];
		buf->len = BUFFER_SIZE;
	}, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
		auto socket = static_cast<Socket*>(stream->data);
		StreamRead& stream_read = socket->stream_read;
		if (nread < 0) {
			if (nread != UV_EOF) {
				Output::Warning("Read error: {}", uv_strerror(nread));
			}
			socket->InternalClose();
		} else if (nread > 0) {
			stream_read.Handle(buf->base, nread);
		}
		if (buf->base) {
			delete[] buf->base;
		}
	});
	OnOpen();
}

void Socket::Close() {
	std::lock_guard lock(m_call_mutex);
	if (!is_initialized)
		return;
	m_request_queue.push(AsyncCall::CLOSE);
	uv_async_send(&async);
}

void Socket::InternalClose() {
	if (uv_is_closing(reinterpret_cast<uv_handle_t*>(&stream)))
		return;
	uv_close(reinterpret_cast<uv_handle_t*>(&stream), [](uv_handle_t* handle) {
		auto socket = static_cast<Socket*>(handle->data);
		uv_close(reinterpret_cast<uv_handle_t*>(&socket->async), nullptr);
		socket->is_initialized = false;
		socket->m_send_queue = decltype(m_send_queue){};
		socket->is_sending = false;
		socket->OnClose();
	});
}

/**
 * ConnectorSocket
 */

void ConnectorSocket::Connect(const std::string_view _host, const uint16_t _port) {
	if (is_connect)
		return;
	is_connect = true;

	addr_host = _host;
	addr_port = _port;

	OnOpen = OnConnect;
	OnClose = [this]() {
		// call uv_stop
		async_data.stop_flag = true;
		uv_async_send(&async);
		if (!manually_close_flag)
			OnDisconnect();
	};

	std::thread([this]() {
		uv_loop_t loop;
		uv_connect_t connect_req;

		connect_req.data = this;

		err = uv_loop_init(&loop);
		if (err < 0) {
			is_connect = false;
			Output::Warning("Loop initialization error: {}", uv_strerror(err));
			return;
		}

		async_data.stop_flag = false;
		async.data = &async_data;

		uv_async_init(&loop, &async, [](uv_async_t *handle) {
			auto async_data = static_cast<AsyncData*>(handle->data);
			if (async_data->stop_flag)
				uv_stop(handle->loop);
		});

		auto Cleanup = [this, &loop]() {
			uv_close(reinterpret_cast<uv_handle_t*>(&async), nullptr);
			uv_run(&loop, UV_RUN_DEFAULT); // update state
			// everying is done, reverse the uv_loop_init
			err = uv_loop_close(&loop);
			if (err) {
				Output::Warning("Close loop error: {}", uv_strerror(err));
			}
			is_connect = false;
		};

		// Call this if not connected
		auto CloseDirectly = [this, Cleanup] () {
			Cleanup();
			OnDisconnect();
		};

		struct sockaddr_storage addr;
		err = Resolve(addr_host, addr_port, &loop, &addr);
		if (err < 0) {
			Output::Warning("Address Resolve error: {}", uv_strerror(err));
			CloseDirectly();
			return;
		}
		InitStream(&loop);
		uv_tcp_connect(&connect_req, GetStream(), reinterpret_cast<struct sockaddr*>(&addr),
				[](uv_connect_t *connect_req, int status) {
			auto socket = static_cast<Socket*>(connect_req->data);
			if (status < 0) {
				Output::Warning("Connection error: {}", uv_strerror(status));
				socket->Close();
				return;
			}
			socket->Open();
		});

		uv_run(&loop, UV_RUN_DEFAULT);

		Cleanup();
	}).detach();
}

void ConnectorSocket::Disconnect() {
	manually_close_flag = true;
	Close();
}

/**
 * ServerListener
 */

void ServerListener::Start(bool wait_thread) {
	if (is_running)
		return;
	is_running = true;

	std::thread thread([this]() {
		int err;

		err = uv_loop_init(&loop);
		if (err < 0) {
			is_running = false;
			Output::Warning("Loop initialization error: {}", uv_strerror(err));
			return;
		}

		async_data.stop_flag = false;
		async.data = &async_data;

		uv_async_init(&loop, &async, [](uv_async_t *handle) {
			auto async_data = static_cast<AsyncData*>(handle->data);
			if (async_data->stop_flag)
				uv_stop(handle->loop);
		});

		uv_tcp_t listener;
		listener.data = this;

		uv_tcp_init(&loop, &listener);

		auto Cleanup = [this, &listener]() {
			uv_close(reinterpret_cast<uv_handle_t*>(&async), nullptr);
			uv_close(reinterpret_cast<uv_handle_t*>(&listener), nullptr);
			uv_run(&loop, UV_RUN_DEFAULT);
			int err = uv_loop_close(&loop);
			if (err) {
				Output::Warning("Close loop error: {}", uv_strerror(err));
			}
			is_running = false;
		};

		struct sockaddr_storage addr;
		err = Resolve(addr_host, addr_port, &loop, &addr);
		if (err < 0) {
			Output::Warning("Address Resolve error: {}", uv_strerror(err));
			Cleanup();
			return;
		}

		err = uv_tcp_bind(&listener, reinterpret_cast<struct sockaddr*>(&addr), 0);
		if (err) {
			Output::Warning("Binding error: {}", uv_strerror(err));
			Cleanup();
			return;
		}

		err = uv_listen(reinterpret_cast<uv_stream_t*>(&listener), 4096, [](uv_stream_t *listener, int status) {
			auto server_listener = static_cast<ServerListener*>(listener->data);
			if (status < 0) {
				return;
			}
			std::unique_ptr<Socket> socket = std::make_unique<Socket>();
			socket.reset(new Socket());
			socket->InitStream(&server_listener->loop);
			if (uv_accept(listener, reinterpret_cast<uv_stream_t*>(socket->GetStream())) == 0) {
				server_listener->OnConnection(std::move(socket));
			} else {
				socket->Close();
			}
		});
		if (err) {
			Output::Warning("Listen error: {}", uv_strerror(err));
			Cleanup();
			return;
		}

		uv_run(&loop, UV_RUN_DEFAULT);

		Cleanup();
	});

	if (wait_thread)
		thread.join();
	else
		thread.detach();
}

void ServerListener::Stop() {
	if (!is_running)
		return;
	async_data.stop_flag = true;
	uv_async_send(&async);
}
