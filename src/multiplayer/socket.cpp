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

/**
 * Socket
 */

Socket::Socket() {
	stream_read.socket = this;
	send_req.data = this;
}

void Socket::Send(std::string_view& data) {
	if (m_send_queue.size() > 100)
		return;
	const uint16_t data_size = data.size();
	const uint16_t final_size = HEAD_SIZE+data_size;
	auto buf = new std::vector<char>(final_size);
	std::memcpy(buf->data(), &data_size, HEAD_SIZE);
	std::memcpy(buf->data()+HEAD_SIZE, data.data(), data_size);
	m_send_queue.emplace(buf);
	if (!is_sending) {
		is_sending = true;
		ContinueSend();
	}
}

void Socket::ContinueSend() {
	auto& vec_buf = m_send_queue.front();
	uv_buf_t buf = uv_buf_init(vec_buf->data(), vec_buf->size());
	uv_write(&send_req, reinterpret_cast<uv_stream_t*>(&stream), &buf, 1,
			[](uv_write_t* req, int err) {
		auto socket = static_cast<Socket*>(req->data);
		if (err) {
			Output::Debug("Error writing to the stream: {}", uv_strerror(err));
		}
		socket->m_send_queue.pop();
		if (socket->m_send_queue.empty()) {
			socket->is_sending = false;
		} else {
			socket->ContinueSend();
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
	OnOpen();
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
			socket->Close();
		} else if (nread > 0) {
			stream_read.Handle(buf->base, nread);
		}
		if (buf->base) {
			delete[] buf->base;
		}
	});
}

void Socket::Close() {
	if (uv_is_closing(reinterpret_cast<uv_handle_t*>(&stream)))
		return;
	uv_close(reinterpret_cast<uv_handle_t*>(&stream), [](uv_handle_t* handle) {
		auto socket = static_cast<Socket*>(handle->data);
		socket->m_send_queue = decltype(m_send_queue){};
		socket->is_sending = false;
		socket->OnClose();
	});
}

/**
 * ConnectorSocket
 */

void ConnectorSocket::Connect(const std::string_view _host, const uint16_t _port) {
	if (is_connected)
		return;
	is_connected = true;

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
			Output::Warning("Loop initialization error: {}", uv_strerror(err));
			return;
		}

		async_data.loop = &loop;
		async_data.stop_flag = false;
		async.data = &async_data;

		uv_async_init(&loop, &async, [](uv_async_t *handle) {
			auto async_data = static_cast<AsyncData*>(handle->data);
			if (async_data->stop_flag)
				uv_stop(async_data->loop);
		});

		auto Cleanup = [this, &loop]() {
			uv_close(reinterpret_cast<uv_handle_t*>(&async), nullptr);
			uv_run(&loop, UV_RUN_DEFAULT); // update state
			// everying is done, reverse the uv_loop_init
			err = uv_loop_close(&loop);
			if (err) {
				Output::Warning("Close loop error: {}", uv_strerror(err));
			}
			is_connected = false;
		};

		// Call this if not connected
		auto CloseDirectly = [this, Cleanup] () {
			Cleanup();
			OnDisconnect();
		};

		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AF_UNSPEC;
		uv_getaddrinfo_t req;
		err = uv_getaddrinfo(&loop, &req, nullptr, addr_host.c_str(),
				std::to_string(addr_port).c_str(), &hints);
		if (err < 0) {
			Output::Warning("Address Resolve error: {}", uv_strerror(err));
			CloseDirectly();
			return;
		}
		InitStream(&loop);
		uv_tcp_connect(&connect_req, GetStream(), req.addrinfo->ai_addr,
				[](uv_connect_t *connect_req, int status) {
			auto socket = static_cast<Socket*>(connect_req->data);
			if (status < 0) {
				Output::Warning("Connection error: {}", uv_strerror(status));
				socket->Close();
				return;
			}
			socket->Open();
		});
		uv_freeaddrinfo(req.addrinfo);

		uv_run(&loop, UV_RUN_DEFAULT);

		Cleanup();
	}).detach();
}

void ConnectorSocket::Disconnect() {
	manually_close_flag = true;
	Close();
	// Send a noop message to trigger the close_cb of uv_close
	uv_async_send(&async);
}
