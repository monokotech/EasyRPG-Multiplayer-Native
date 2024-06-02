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
#include "util/serialize.h"
#include "socket.h"
#include "connection.h"
#include "socks5.h"

/**
 * libuv examples:
 * https://github.com/kurotych/libuv-echo-server master:a25a369
 * https://github.com/cm-MMK-2/MTcpServer master:48ae4e6
 *
 * name resolver:
 * nodejs: master:b876e00 inspector_socket_server.cc: InspectorSocketServer::Start
 * minetest: 5.7.0:1b95998 address.cpp Address::Resolve
 *
 * misc stuff:
 * https://stackoverflow.com/questions/25615340/closing-libuv-handles-correctly/25831688#25831688
 * https://stackoverflow.com/questions/56720702/call-uv-write-from-multi-thread-its-callback-never-get-called
 */

std::string GetPeerAddress(uv_tcp_t* handle) {
	/** References
	 * https://github.com/HaxeFoundation/hashlink/issues/226#issuecomment-460034707
	 * https://stackoverflow.com/questions/46647941/how-do-i-perform-a-dns-look-up-using-libuv */
	struct sockaddr_storage addr;
	int addr_len = sizeof(addr);
	int err = uv_tcp_getpeername(handle,
			reinterpret_cast<struct sockaddr*>(&addr), &addr_len);
	if (!err) {
		char host[40];
		uint16_t port;
		memset(host, 0, sizeof(host));
		if (addr.ss_family == AF_INET) {
			struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(&addr);
			uv_ip4_name(addr_in, host, 16);
			port = ntohs(addr_in->sin_port);
		} else if (addr.ss_family == AF_INET6) {
			struct sockaddr_in6* addr_in6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
			uv_ip6_name(addr_in6, host, 39);
			port = ntohs(addr_in6->sin6_port);
		}
		return std::string(host) + " " + std::to_string(port);
	}
	return std::string("addr err = ") + uv_strerror(err);
}

int Resolve(const std::string& address, const uint16_t port,
		uv_loop_t* loop, struct sockaddr_storage* addr) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AF_UNSPEC;
	uv_getaddrinfo_t req;
	int err = uv_getaddrinfo(loop, &req, nullptr, address.c_str(),
			std::to_string(port).c_str(), &hints);
	if (!err) {
		if (req.addrinfo->ai_family == AF_INET)
			memcpy(addr, req.addrinfo->ai_addr, sizeof(struct sockaddr_in));
		else if (req.addrinfo->ai_family == AF_INET6)
			memcpy(addr, req.addrinfo->ai_addr, sizeof(struct sockaddr_in6));
	}
	if (req.addrinfo) {
		uv_freeaddrinfo(req.addrinfo);
	}
	return err;
}

/**
 * DataHandler
 * + Resolve the issue of TCP packet sticking or unpacking
 */

void DataHandler::Send(std::string_view data) {
	OnWrite(SerializeString16(data));
}

void DataHandler::GotDataBuffer(const char* buf, size_t buf_used) {
	while (begin < buf_used) {
		uint16_t buf_remaining = buf_used-begin;
		uint16_t tmp_buf_remaining = BUFFER_SIZE-tmp_buf_used;
		if (tmp_buf_used > 0) {
			if (got_head) {
				uint16_t data_remaining = data_size-tmp_buf_used;
				// There is enough temporary space to write into
				if (data_remaining <= tmp_buf_remaining) {
					if (data_remaining <= buf_remaining) {
						memcpy(tmp_buf+tmp_buf_used, buf+begin, data_remaining);
						OnMessageBuffer(tmp_buf, data_size);
						begin += data_remaining;
					} else {
						if (buf_remaining > 0) {
							memcpy(tmp_buf+tmp_buf_used, buf+begin, buf_remaining);
							tmp_buf_used += buf_remaining;
						}
						break; // Go to the next packet
					}
				} else {
					OnWarning(std::string("DataHandler::GotDataBuffer exception (data): ")
						.append("tmp_buf_used: ").append(std::to_string(tmp_buf_used))
						.append(", data_size: ").append(std::to_string(data_size))
						.append(", data_remaining: ").append(std::to_string(data_remaining)));
				}
				got_head = false;
				tmp_buf_used = 0;
				data_size = 0;
			} else {
				uint16_t head_remaining = HEAD_SIZE-tmp_buf_used;
				if (head_remaining <= buf_remaining && head_remaining <= tmp_buf_remaining) {
					memcpy(tmp_buf+tmp_buf_used, buf+begin, head_remaining);
					data_size = ReadU16((uint8_t*)tmp_buf);
					begin += head_remaining;
					got_head = true;
				}
				tmp_buf_used = 0;
			}
		} else {
			// The header can be taken out from buf_remaining
			if (!got_head && HEAD_SIZE <= buf_remaining) {
				data_size = ReadU16((uint8_t*)buf+begin);
				begin += HEAD_SIZE;
				got_head = true;
			// Data can be taken out from buf_remaining
			} else if (got_head && data_size <= buf_remaining) {
				OnMessageBuffer(buf+begin, data_size);
				begin += data_size;
				got_head = false;
				data_size = 0;
			} else if (buf_remaining < HEAD_SIZE || buf_remaining < data_size) {
				if (buf_remaining > 0 && buf_remaining <= tmp_buf_remaining) {
					memcpy(tmp_buf+tmp_buf_used, buf+begin, buf_remaining);
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


void DataHandler::Close() {
	OnClose();
}


/**
 * Socket
 */

Socket::Socket() {
	send_req.data = this;
	// Wrapper is needed, the callback has not been assigned yet
	data_handler.OnWrite = [this](std::string_view data) { Write(data); };
	data_handler.OnMessage = [this](std::string_view data) { OnMessage(data); };
	data_handler.OnClose = [this]() { CloseSocket(); };
	data_handler.OnWarning = [this](std::string_view data) { OnWarning(data); };
}

void Socket::InitStream(uv_loop_t* loop) {
	stream.data = this;
	async_data.socket = this;
	async.data = &async_data;
	uv_async_init(loop, &async, [](uv_async_t* handle) {
		auto async_data = static_cast<AsyncData*>(handle->data);
		auto socket = async_data->socket;
		std::lock_guard lock(socket->m_call_mutex);
		while (!socket->m_request_queue.empty()) {
			switch (socket->m_request_queue.front()) {
			case AsyncCall::WRITE:
				if (!socket->is_sending &&
						!socket->m_send_queue.empty()) {
					socket->is_sending = true;
					socket->InternalWrite();
				}
				break;
			case AsyncCall::OPENSOCKET:
				socket->InternalOpenSocket();
				break;
			case AsyncCall::CLOSESOCKET:
				socket->InternalCloseSocket();
				break;
			}
			socket->m_request_queue.pop();
		}
	});
	uv_tcp_init(loop, &stream);
	read_timeout_req.data = this;
	uv_timer_init(loop, &read_timeout_req);
	is_initialized = true;
}

void Socket::Write(std::string_view data) {
	std::lock_guard lock(m_call_mutex);

	if (!is_initialized || m_send_queue.size() > 100)
		return;

	m_send_queue.emplace(data);

	m_request_queue.push(AsyncCall::WRITE);
	uv_async_send(&async);
}

void Socket::InternalWrite() {
	std::string_view data = m_send_queue.front();
	uv_buf_t buf = uv_buf_init(const_cast<char*>(data.data()), data.size());
	uv_write(&send_req, reinterpret_cast<uv_stream_t*>(&stream), &buf, 1,
			[](uv_write_t* req, int err) {
		auto socket = static_cast<Socket*>(req->data);
		if (err) {
			socket->OnWarning(std::string("Writing to the stream failed: ").append(uv_strerror(err)));
		}
		if (socket->is_sending) {
			socket->m_send_queue.pop();
			if (socket->m_send_queue.empty()) {
				socket->is_sending = false;
			} else {
				socket->InternalWrite();
			}
		}
	});
}

void Socket::Open() {
	std::lock_guard lock(m_call_mutex);
	m_request_queue.push(AsyncCall::OPENSOCKET);
	uv_async_send(&async);
}

void Socket::InternalOpenSocket() {
	uv_read_start(reinterpret_cast<uv_stream_t*>(&stream),
			[](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
		buf->base = new char[DataHandler::BUFFER_SIZE];
		buf->len = DataHandler::BUFFER_SIZE;
	}, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
		auto socket = static_cast<Socket*>(stream->data);
		DataHandler& data_handler = socket->data_handler;
		if (nread < 0) {
			// EOF means the connection is just closed remotely,
			//  so do not output warnings
			if (nread != UV_EOF) {
				socket->OnWarning(std::string("Read failed: ").append(uv_strerror(nread)));
			}
			socket->InternalCloseSocket();
		} else if (nread > 0) {
			if (socket->OnData) {
				socket->OnData(std::string_view(buf->base, nread));
			} else {
				data_handler.GotDataBuffer(buf->base, nread);
			}
		}
		if (buf->base) {
			delete[] buf->base;
		}
		if (socket->read_timeout_ms > 0)
			uv_timer_again(&socket->read_timeout_req);
	});
	if (read_timeout_ms > 0) {
		uv_timer_start(&read_timeout_req, [](uv_timer_t* handle) {
			auto socket = static_cast<Socket*>(handle->data);
			socket->OnWarning(std::string("Read timeout: ").append(GetPeerAddress(&socket->stream)));
			socket->InternalCloseSocket();
		}, read_timeout_ms, read_timeout_ms);
	}
	OnInfo(std::string("Created a connection from: ").append(GetPeerAddress(&stream)));
	OnOpen();
}

void Socket::CloseSocket() {
	std::lock_guard lock(m_call_mutex);
	if (!is_initialized)
		return;
	m_request_queue.push(AsyncCall::CLOSESOCKET);
	uv_async_send(&async);
}

void Socket::InternalCloseSocket() {
	if (uv_is_closing(reinterpret_cast<uv_handle_t*>(&stream)))
		return;
	OnInfo(std::string("Closing connection: ").append(GetPeerAddress(&stream)));
	uv_timer_stop(&read_timeout_req);
	uv_close(reinterpret_cast<uv_handle_t*>(&stream), [](uv_handle_t* handle) {
		auto socket = static_cast<Socket*>(handle->data);
		uv_close(reinterpret_cast<uv_handle_t*>(&socket->read_timeout_req), nullptr);
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

void ConnectorSocket::SetRemoteAddress(std::string_view host, const uint16_t port) {
	addr_host = host;
	addr_port = port;
}

void ConnectorSocket::ConfigSocks5(std::string_view host, const uint16_t port) {
	if (!host.empty()) {
		socks5_req_addr_host = addr_host;
		socks5_req_addr_port = addr_port;
		addr_host = host;
		addr_port = port;
	}
}

void ConnectorSocket::Connect() {
	if (is_connect)
		return;
	is_connect = true;
	is_failed = false;

	manually_close_flag = false;

	OnOpen = [this]() {
		if (socks5_req_addr_host.empty())
			OnConnect();
	};
	OnClose = [this]() {
		// call uv_stop
		async_data.stop_flag = true;
		uv_async_send(&async);
	};

	std::thread([this]() {
		int err;

		uv_loop_t loop;
		uv_connect_t connect_req;

		connect_req.data = this;

		err = uv_loop_init(&loop);
		if (err < 0) {
			is_connect = false;
			OnWarning(std::string("Loop initialization failed: ").append(uv_strerror(err)));
			return;
		}

		async_data.stop_flag = false;
		async.data = &async_data;

		uv_async_init(&loop, &async, [](uv_async_t* handle) {
			auto async_data = static_cast<AsyncData*>(handle->data);
			if (async_data->stop_flag)
				uv_stop(handle->loop);
		});

		auto Cleanup = [this, &loop]() {
			uv_close(reinterpret_cast<uv_handle_t*>(&async), nullptr);
			uv_run(&loop, UV_RUN_DEFAULT); // update state
			// everying is done, reverse the uv_loop_init
			int err = uv_loop_close(&loop);
			if (err) {
				is_failed = true;
				OnWarning(std::string("Close loop failed: ").append(uv_strerror(err)));
			}
			is_connect = false;
			if (!manually_close_flag) {
				if (is_failed)
					OnFail();
				else
					OnDisconnect();
			}
		};

		struct sockaddr_storage addr;
		err = Resolve(addr_host, addr_port, &loop, &addr);
		if (err < 0) {
			is_failed = true;
			OnWarning(std::string("Address Resolve failed: ").append(uv_strerror(err)));
			Cleanup();
			return;
		}
		InitStream(&loop);
		uv_tcp_connect(&connect_req, GetStream(), reinterpret_cast<struct sockaddr*>(&addr),
				[](uv_connect_t* connect_req, int status) {
			auto socket = static_cast<ConnectorSocket*>(connect_req->data);
			if (status < 0) {
				socket->is_failed = true;
				socket->OnWarning(std::string("Connection failed: ").append(uv_strerror(status)));
				socket->CloseSocket();
				return;
			}
			socket->Open();
			if (!socket->socks5_req_addr_host.empty()) {
				socket->OnData = [socket](std::string_view data) {
					switch (socket->socks5_step) {
					case SOCKS5_STEP::SS_GREETING:
						if (!SOCKS5::CheckGreeting(std::vector<char>(data.begin(), data.end()))) {
							auto vec = SOCKS5::GetConnectionRequest(
									socket->socks5_req_addr_host, socket->socks5_req_addr_port);
							socket->Write(std::string_view(vec.data(), vec.size()));
							socket->socks5_step = SOCKS5_STEP::SS_CONNECTIONREQUEST;
							return;
						}
						break;
					case SOCKS5_STEP::SS_CONNECTIONREQUEST:
						if (!SOCKS5::CheckConnectionRequest(std::vector<char>(data.begin(), data.end()))) {
							socket->OnInfo(std::string("SOCKS5 request successful: ")
								.append(socket->socks5_req_addr_host)
								.append(":")
								.append(std::to_string(socket->socks5_req_addr_port)));
							socket->OnData = nullptr;
							socket->OnConnect();
							return;
						}
						break;
					}
					socket->is_failed = true;
					socket->OnWarning(std::string("SOCKS5 request failed at step: ").append(
							std::to_string(static_cast<uint8_t>(socket->socks5_step))));
					socket->CloseSocket();
				};
				auto vec = SOCKS5::GetGreeting();
				socket->Write(std::string_view(vec.data(), vec.size()));
				socket->socks5_step = SOCKS5_STEP::SS_GREETING;
			}
		});

		uv_run(&loop, UV_RUN_DEFAULT);

		Cleanup();
	}).detach();
}

void ConnectorSocket::Disconnect() {
	manually_close_flag = true;
	CloseSocket();
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
			OnWarning(std::string("Loop initialization failed: ").append(uv_strerror(err)));
			return;
		}

		async_data.stop_flag = false;
		async.data = &async_data;

		uv_async_init(&loop, &async, [](uv_async_t* handle) {
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
				OnWarning(std::string("Close loop failed: ").append(uv_strerror(err)));
			}
			is_running = false;
		};

		struct sockaddr_storage addr;
		err = Resolve(addr_host, addr_port, &loop, &addr);
		if (err < 0) {
			OnWarning(std::string("Address Resolve failed: ").append(uv_strerror(err)));
			Cleanup();
			return;
		}

		err = uv_tcp_bind(&listener, reinterpret_cast<struct sockaddr*>(&addr), 0);
		if (err) {
			OnWarning(std::string("Binding failed: ").append(uv_strerror(err)));
			Cleanup();
			return;
		}

		OnInfo("Listening on: " + addr_host + " " + std::to_string(addr_port));

		err = uv_listen(reinterpret_cast<uv_stream_t*>(&listener), 4096, [](uv_stream_t* listener, int status) {
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
				socket->CloseSocket();
			}
		});
		if (err) {
			OnWarning(std::string("Listen failed: ").append(uv_strerror(err)));
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
