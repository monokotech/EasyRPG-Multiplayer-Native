#include "tcp6_socket.h"
#include <thread>
#include <vector>
#include "connection.h"

constexpr size_t MAX_QUEUE_SIZE = Multiplayer::Connection::MAX_QUEUE_SIZE;

void TCP6Socket::InitSocket(const sockpp::tcp6_socket& socket) {
	read_socket = std::move(socket.clone());
	write_socket = std::move(socket.clone());
}

void TCP6Socket::Send(std::string_view& data) {
	const uint16_t data_size = data.size();
	const uint16_t final_size = HEAD_SIZE+data_size;
	std::vector<char> buf(final_size);
	std::memcpy(buf.data(), &data_size, HEAD_SIZE);
	std::memcpy(buf.data()+HEAD_SIZE, data.data(), data_size);
	if (write_socket.write(buf.data(), final_size) != final_size) {
		if (write_socket.last_error() == EPIPE) {
			OnLogDebug(LABEL + ": It appears that the write_socket v6 was closed.");
			Close();
			OnClose();
		} else {
			OnLogDebug(LABEL + ": Error writing to the TCP stream v6 "
				+ std::string("[") + std::to_string(write_socket.last_error()) + std::string("]: ")
				+ write_socket.last_error_str());
		}
	}
}

void TCP6Socket::CreateConnectionThread(const size_t read_timeout_seconds) {
	std::thread([this, read_timeout_seconds]() {
		OnOpen();
		OnLogDebug(LABEL + ": Created a connection from: "
			+ read_socket.peer_address().to_string());

		char buf[MAX_QUEUE_SIZE];
		ssize_t buf_used;
		bool close = false;

		if (read_timeout_seconds > 0 &&
				!read_socket.read_timeout(std::chrono::seconds(read_timeout_seconds))) {
			OnLogWarning(LABEL + ": Failed to set the timeout on read_socket v6 stream: "
				+ read_socket.last_error_str());
		}

		bool got_head = false;
		uint16_t data_size = 0;
		uint16_t begin = 0;
		char tmp_buf[MAX_QUEUE_SIZE];
		uint16_t tmp_buf_used = 0;

		while (true) {
			buf_used = read_socket.read(buf, sizeof(buf));
			if (buf_used > 0) {
				// desegment
				while (begin < buf_used) {
					uint16_t buf_remaining = buf_used-begin;
					uint16_t tmp_buf_remaining = MAX_QUEUE_SIZE-tmp_buf_used;
					if (tmp_buf_used > 0) {
						if (got_head) {
							uint16_t data_remaining = data_size-tmp_buf_used;
							// has tmp room to write
							if (data_remaining <= tmp_buf_remaining) {
								if (data_remaining <= buf_remaining) {
									std::memcpy(tmp_buf+tmp_buf_used, buf+begin, data_remaining);
									OnData(tmp_buf, data_size);
									begin += data_remaining;
								} else {
									if (buf_remaining > 0) {
										std::memcpy(tmp_buf+tmp_buf_used, buf+begin, buf_remaining);
										tmp_buf_used += buf_remaining;
									}
									break; // to the next packet
								}
							} else {
								OnLogDebug(LABEL + ": Exception (data): "
									+ std::string("tmp_buf_used: ") + std::to_string(tmp_buf_used)
									+ std::string(", data_size: ") + std::to_string(data_size)
									+ std::string(", data_remaining: ") + std::to_string(data_remaining));
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
						// can take HEAD_SIZE from buf_remaining
						if (!got_head && HEAD_SIZE <= buf_remaining) {
							std::memcpy(&data_size, buf+begin, HEAD_SIZE);
							begin += HEAD_SIZE;
							got_head = true;
						// can take data_size from buf_remaining
						} else if (got_head && data_size <= buf_remaining) {
							OnData(buf+begin, data_size);
							begin += data_size;
							got_head = false;
							data_size = 0;
						} else if (buf_remaining < HEAD_SIZE || buf_remaining < data_size) {
							if (buf_remaining > 0 && buf_remaining <= tmp_buf_remaining) {
								std::memcpy(tmp_buf+tmp_buf_used, buf+begin, buf_remaining);
								tmp_buf_used += buf_remaining;
							}
							break; // to the next packet
						}
					}
					// skip empty messages
					if (got_head && data_size == 0) {
						got_head = false;
					}
				}
				begin = 0;
			} else if (buf_used == 0) {
				break;
			} else {
				OnLogDebug(LABEL + ": Read error "
					+ std::string("[") + std::to_string(read_socket.last_error()) + std::string("]: ")
					+ read_socket.last_error_str());
				close = true;
				break;
			}
		}

		if (!close_silently)
			OnLogDebug(LABEL + ": Connection closed from: "
				+ read_socket.peer_address().to_string());
		if (close)
			read_socket.close();
		if (!close_silently)
			OnClose();
	}).detach();
}

void TCP6Socket::Close() {
	close_silently = true;
	read_socket.shutdown();
}

/* gdb --command=~/gdb
define ad
	display tmp_buf_used
	display got_head
	display data_size
	display begin
	display buf_used
	display buf
end
define ab
	b tcp_socket.cpp:15 if data_size > 4096
	b tcp_socket.cpp:100 if data_size > 4096
end
*/
