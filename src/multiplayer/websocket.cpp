/*
 * EPMP
 * Copyright (C) 2024 Monokotech
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   MIT License
 *   Copyright (c) 2021 Matheus Valadares
 *
 *   Full text: https://github.com/Matheus28/ws28/blob/13301db/LICENSE
 */

// ws28 master:13301db

#include "websocket.h"
#include <cstddef> // size_t
#include <cstring> // memmove
#include <optional>
#include <map> // std::multimap
#include <string> // std::string
#include <cctype> // std::tolower
#include <algorithm> // std::transform
#include <sstream> // std::stringstream
#include "util/sha1.h"
#include "util/base64.h"

namespace {

constexpr size_t MAX_MESSAGE_SIZE = 4096;
constexpr size_t MAX_HEADER_SIZE = 10;
constexpr size_t MASK_KEY_SIZE = 4;
constexpr size_t MAX_FRAME_SIZE = MAX_HEADER_SIZE + MASK_KEY_SIZE + MAX_MESSAGE_SIZE;

namespace Detail {
	bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
		if (a.size() != b.size()) return false;
		for (;;) {
			if (std::tolower(a.front()) != std::tolower(b.front())) return false;
			a.remove_prefix(1);
			b.remove_prefix(1);
			if (a.empty()) return true;
		}
	}

	bool EqualsIgnoreCase(std::string_view a, std::string_view b, size_t n) {
		while (n--) {
			if (a.empty()) {
				return b.empty();
			} else if (b.empty()) {
				return false;
			}
			if (std::tolower(a.front()) != std::tolower(b.front())) return false;
			a.remove_prefix(1);
			b.remove_prefix(1);
		}
		return true;
	}

	bool HeaderContains(std::string_view header, std::string_view substring) {
		bool has_match = false;
		while (!header.empty()) {
			if (header.front() == ' ' || header.front() == '\t') {
				header.remove_prefix(1);
				continue;
			}
			if (has_match) {
				if (header.front() == ',') return true;
				has_match = false;
				header.remove_prefix(1);
				// Skip to comma or end of string
				while (!header.empty() && header.front() != ',') header.remove_prefix(1);
				if (header.empty()) return false;
				// Skip comma
				header.remove_prefix(1);
			} else {
				if (Detail::EqualsIgnoreCase(header, substring, substring.size())) {
					// We have a match... if the header ends here, or has a comma
					has_match = true;
					header.remove_prefix(substring.size());
				} else {
					header.remove_prefix(1);
					// Skip to comma or end of string
					while (!header.empty() && header.front() != ',') header.remove_prefix(1);
					if (header.empty()) return false;
					// Skip comma
					header.remove_prefix(1);
				}
			}
		}
		return has_match;
	}
} // end of namespace Detail

class RequestHeaders {
public:
	void Set(std::string_view key, std::string_view value) {
		m_headers.push_back({ key, value });
	}

	template<typename F>
	void ForEachValueOf(std::string_view key, const F &f) const {
		for (auto &p : m_headers) {
			if (p.first == key) f(p.second);
		}
	}

	std::optional<std::string_view> Get(std::string_view key) const {
		for (auto &p : m_headers) {
			if (p.first == key) return p.second;
		}
		return std::nullopt;
	}

	template<typename F>
	void ForEach(const F &f) const {
		for (auto &p : m_headers) {
			f(p.first, p.second);
		}
	}

private:
	std::vector<std::pair<std::string_view, std::string_view>> m_headers;
};

class HTTPResponse {
public:
	HTTPResponse& SetStatus(int v) {
		status_code = v;
		return *this;
	}

	/**
	 * Appends a response header. The following headers cannot be changed:
	 * + Connection: close
	 * + Content-Length: body.size()
	 */
	HTTPResponse& SetHeader(std::string_view key, std::string_view value) {
		headers.emplace(std::string(key), std::string(value));
		return *this;
	}

	HTTPResponse& AppendBody(std::string_view v) {
		body.append(v);
		return *this;
	}

	int status_code = 200;
	std::string body;
	std::multimap<std::string, std::string> headers;
};

struct DataFrameHeader {
	char* buf;

	DataFrameHeader(char* _buf) : buf(_buf) {}

	void reset() {
		buf[0] = 0;
		buf[1] = 0;
	}

	// Read
	bool fin() { return (buf[0] >> 7) & 1; }
	bool rsv1() { return (buf[0] >> 6) & 1; }
	bool rsv2() { return (buf[0] >> 5) & 1; }
	bool rsv3() { return (buf[0] >> 4) & 1; }
	bool mask() { return (buf[1] >> 7) & 1; }
	uint8_t opcode() {
		return buf[0] & 0x0F;
	}
	uint8_t len() {
		return buf[1] & 0x7F;
	}

	// Write
	void fin(bool v) { buf[0] &= ~(1 << 7); buf[0] |= v << 7; }
	void rsv1(bool v) { buf[0] &= ~(1 << 6); buf[0] |= v << 6; }
	void rsv2(bool v) { buf[0] &= ~(1 << 5); buf[0] |= v << 5; }
	void rsv3(bool v) { buf[0] &= ~(1 << 4); buf[0] |= v << 4; }
	void mask(bool v) { buf[1] &= ~(1 << 7); buf[1] |= v << 7; }
	void opcode(uint8_t v) {
		buf[0] &= ~0x0F;
		buf[0] |= v & 0x0F;
	}
	void len(uint8_t v) {
		buf[1] &= ~0x7F;
		buf[1] |= v & 0x7F;
	}
};

/**
 * Functions
 */

void WriteDataFrameHeader(uint8_t opcode, uint64_t len, char* header_start) {
	DataFrameHeader header{ header_start };
	header.reset();
	header.fin(true);
	header.opcode(opcode);
	header.mask(false);
	header.rsv1(false);
	header.rsv2(false);
	header.rsv3(false);
	if (len >= 126) {
		if (len > UINT16_MAX) {
			header.len(127);
			*(uint8_t*)(header_start + 2) = (len >> 56) & 0xFF;
			*(uint8_t*)(header_start + 3) = (len >> 48) & 0xFF;
			*(uint8_t*)(header_start + 4) = (len >> 40) & 0xFF;
			*(uint8_t*)(header_start + 5) = (len >> 32) & 0xFF;
			*(uint8_t*)(header_start + 6) = (len >> 24) & 0xFF;
			*(uint8_t*)(header_start + 7) = (len >> 16) & 0xFF;
			*(uint8_t*)(header_start + 8) = (len >> 8) & 0xFF;
			*(uint8_t*)(header_start + 9) = (len >> 0) & 0xFF;
		} else {
			header.len(126);
			*(uint8_t*)(header_start + 2) = (len >> 8) & 0xFF;
			*(uint8_t*)(header_start + 3) = (len >> 0) & 0xFF;
		}
	} else {
		header.len(len);
	}
}

size_t GetDataFrameHeaderSize(uint64_t len) {
	if (len >= 126) {
		if (len > UINT16_MAX) {
			return 10;
		} else {
			return 4;
		}
	} else {
		return 2;
	}
}

} // end of namespace

void WebSocket::Send(std::string_view data, uint8_t opcode) {
	char header[MAX_HEADER_SIZE];
	WriteDataFrameHeader(opcode, data.size(), header);
	OnWrite(std::string((char*)header, GetDataFrameHeaderSize(data.size()))
			.append(std::string_view((char*)data.data(), data.size())));
}

void WebSocket::GotData(std::string_view data) {
	if (m_buffer.size() > MAX_FRAME_SIZE) {
		if (m_has_completed_handshake) {
			Close(1009, "Message too large");
		} else {
			OnClose();
		}
		return;
	}

	/**
	 * If we don't have anything stored in m_buffer, we use the buffer we received
	 *  in the function argument (data), so we don't have to perform any copying.
	 * The Bail() function needs to be called before we leave this function
	 *  to copy the unused part of the buffer back to m_buffer
	 */
	std::string_view buffer;
	bool using_local_buffer;
	if (m_buffer.empty()) {
		using_local_buffer = true;
		buffer = std::string_view(data.data(), data.size());
	} else {
		using_local_buffer = false;
		m_buffer.insert(m_buffer.end(), data.begin(), data.end());
		buffer = std::string_view(m_buffer.data(), m_buffer.size());
	}
	// Remove the part of the buffer that has already been used
	auto Consume = [&buffer](size_t amount) {
		buffer.remove_prefix(amount);
	};
	// Need to wait for the next GotData() call to get more data
	auto Bail = [this, &using_local_buffer, &buffer]() {
		if (using_local_buffer) {
			// Save data to m_buffer
			if (!buffer.empty()) {
				m_buffer.insert(m_buffer.end(), buffer.begin(), buffer.end());
			}
		} else {
			// Sync m_buffer (likely consumed)
			if (buffer.empty()) {
				m_buffer.clear();
			} else if (buffer.size() != m_buffer.size()) {
				memmove(m_buffer.data(), buffer.data(), buffer.size());
				m_buffer.resize(buffer.size());
			}
		}
	};

	if (!m_has_completed_handshake) {
		auto end_of_headers = buffer.find("\r\n\r\n");

		// HTTP headers not done yet, wait (skip)
		if (end_of_headers == std::string_view::npos) return Bail();

		auto MalformedRequest = [this](std::string_view reason) {
			OnWarning(std::string("WS: MalformedRequest: ").append(reason));
			OnWrite(std::string("HTTP/1.1 400 Bad Request\r\n\r\n").append(reason));
			OnClose();
		};

		// `+ 4` includes "\r\n\r\n"
		auto headers_buffer = buffer.substr(0, end_of_headers + 4);

		// Extract header information
		std::string_view method;
		std::string_view path;
		{
			auto method_end = headers_buffer.find(' ');
			auto line_end = headers_buffer.find("\r\n");

			if (method_end == std::string_view::npos || method_end > line_end) {
				return MalformedRequest("Wrong method_end");
			}

			method = headers_buffer.substr(0, method_end);

			// Uppercase method
			std::transform(method.begin(), method.end(), (char*)method.data(),
					[](char v) -> char {
				if (v < 0 || v >= 127) return v;
				return toupper(v);
			});

			auto path_start = method_end + 1;
			auto path_end = headers_buffer.find(' ', path_start);

			if (path_end == std::string_view::npos || path_end > line_end) {
				return MalformedRequest("Wrong line_end");
			}

			path = headers_buffer.substr(path_start, path_end - path_start);

			// Skip line
			headers_buffer = headers_buffer.substr(line_end + 2); // `+ 2` includes "\r\n"
		}

		// Parse header
		RequestHeaders headers;
		for (;;) {
			auto next_line = headers_buffer.find("\r\n");

			// This means that we have finished parsing the headers
			if (next_line == 0) {
				break;
			}

			// This can't happen... right?
			if (next_line == std::string_view::npos) {
				return MalformedRequest("Wrong next_line");
			}

			auto colon_pos = headers_buffer.find(':');

			if (colon_pos == std::string_view::npos || colon_pos > next_line) {
				return MalformedRequest("Wrong colon_pos");
			}

			auto key = headers_buffer.substr(0, colon_pos);

			// Key to lower case
			std::transform(key.begin(), key.end(), (char*)key.data(),
					[](char v) -> char {
				if (v < 0 || v >= 127) return v;
				return std::tolower(v);
			});

			auto value = headers_buffer.substr(colon_pos + 1, next_line - (colon_pos + 1));

			// Trim spaces
			while (!key.empty() && key.front() == ' ') key.remove_prefix(1);
			while (!key.empty() && key.back() == ' ') key.remove_suffix(1);
			while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
			while (!value.empty() && value.back() == ' ') value.remove_suffix(1);

			headers.Set(key, value);

			headers_buffer = headers_buffer.substr(next_line + 2);
		}

		// Check request and response
		{
			if (auto upgrade = headers.Get("upgrade")) {
				if (!Detail::EqualsIgnoreCase(*upgrade, "websocket")) {
					return MalformedRequest("Non-websocket upgrade");
				}
			} else { // Response when not using WebSocket
				HTTPResponse res;

				if (res.status_code == 0) res.status_code = 404;
				if (res.status_code < 200 || res.status_code >= 1000) res.status_code = 500;

				std::stringstream ss;
				ss << "HTTP/1.1 " << res.status_code << " " << "WS28" << "\r\n";
				ss << "Connection: close\r\n";
				ss << "Content-Length: " << res.body.size() << "\r\n";
				for (auto &p : res.headers) {
					ss << p.first << ": " << p.second << "\r\n";
				}
				ss << "\r\n";
				ss << res.body;
				OnWrite(ss.str());

				OnClose();
				return;
			}
		}

		// Check upgrade
		{
			if (method != "GET") return MalformedRequest("WebSocket upgrades must be GET");

			auto connection_filed = headers.Get("connection");
			if (!connection_filed) return MalformedRequest("Header field 'connection' not found");

			// hackish, ideally we should check it's surrounded by commas (or start/end of string)
			if (!Detail::HeaderContains(*connection_filed, "upgrade")) {
				return MalformedRequest("Wrong upgrade header field: 'connection'");
			}
		}

		// Make upgrade
		{
			bool send_websocket_version = false;
			auto websocket_version = headers.Get("sec-websocket-version");
			if (!websocket_version) return MalformedRequest("Header field 'sec-websocket-version' not found");
			if (!Detail::EqualsIgnoreCase(*websocket_version, "13")) {
				send_websocket_version = true;
			}

			auto websocket_key = headers.Get("sec-websocket-key");
			if (!websocket_key) return MalformedRequest("Header field 'sec-websocket-key' not found");
			std::string security_key = std::string(*websocket_key);
			security_key.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11"); // constant

			bool send_websocket_protocol_binary = false;
			auto websocket_protocol = headers.Get("sec-websocket-protocol");
			if (Detail::EqualsIgnoreCase(*websocket_protocol, "binary")) {
				send_websocket_protocol_binary = true;
			}

			SHA1 sha1;
			sha1.addBytes(security_key);
			std::string base64_hash = base64_encode(sha1.getDigest());

			char buf[256];
			int buf_len = snprintf(buf, sizeof(buf),
				"HTTP/1.1 101 Switching Protocols\r\n"
				"Upgrade: websocket\r\n"
				"Connection: Upgrade\r\n"
				"%s%s"
				"Sec-WebSocket-Accept: %s\r\n\r\n",
				send_websocket_version ? "Sec-WebSocket-Version: 13\r\n" : "",
				send_websocket_protocol_binary ? "Sec-WebSocket-Protocol: binary\r\n" : "",
				base64_hash.c_str()
			);
			OnWrite(std::string(buf, buf_len));
		}

		m_has_completed_handshake = true;
		m_buffer.clear();
		return;
	}

	for (;;) {
		/* Not enough to read the header
		 * + Sync the buffer that has been consumed to m_buffer */
		if (buffer.size() < 2) return Bail();

		DataFrameHeader header((char*)buffer.data());

		if (header.rsv1() || header.rsv2() || header.rsv3()) {
			return Close(1002, "Reserved bit used");
		}

		if (!header.mask()) {
			return Close(1002, "Clients must mask their payload");
		}

		char* cur_char_pos = (char*)buffer.data() + 2;

		uint64_t payload_len = header.len();
		if (payload_len == 126) {
			if (buffer.size() < 4) {
				return Bail();
			}
			payload_len = (*(uint8_t*)(cur_char_pos) << 8) | (*(uint8_t*)(cur_char_pos + 1));
			cur_char_pos += 2;
		} else if (payload_len == 127) {
			if (buffer.size() < 10) {
				return Bail();
			}
			payload_len = ((uint64_t)*(uint8_t*)(cur_char_pos) << 56)
					| ((uint64_t)*(uint8_t*)(cur_char_pos + 1) << 48)
					| ((uint64_t)*(uint8_t*)(cur_char_pos + 2) << 40)
					| ((uint64_t)*(uint8_t*)(cur_char_pos + 3) << 32)
					| (*(uint8_t*)(cur_char_pos + 4) << 24)
					| (*(uint8_t*)(cur_char_pos + 5) << 16)
					| (*(uint8_t*)(cur_char_pos + 6) << 8)
					| (*(uint8_t*)(cur_char_pos + 7) << 0);
			cur_char_pos += 8;
		}

		auto amount_left = buffer.size() - (cur_char_pos - buffer.data());
		const char* mask_key = nullptr;

		// Read mask
		{
			if (amount_left < 4) return Bail();
			mask_key = cur_char_pos;
			cur_char_pos += 4;
			amount_left -= 4;
		}

		if (payload_len > amount_left) return Bail();

		// Unmask
		{
			for (size_t i = 0; i < (payload_len & ~3); i += 4) {
				cur_char_pos[i + 0] ^= mask_key[0];
				cur_char_pos[i + 1] ^= mask_key[1];
				cur_char_pos[i + 2] ^= mask_key[2];
				cur_char_pos[i + 3] ^= mask_key[3];
			}
			for (size_t i = payload_len & ~3; i < payload_len; ++i) {
				cur_char_pos[i] ^= mask_key[i % 4];
			}
		}

		if (header.opcode() >= 0x08) {
			if (!header.fin()) {
				return Close(1002, "Control frame can't be fragmented");
			}
			if (payload_len > 125) {
				return Close(1002, "Control frame can't be more than 125 bytes");
			}
			ProcessDataFrame(header.opcode(), cur_char_pos, payload_len);
		} else if (!IsBuildingFrames() && header.fin()) {
			if (payload_len > MAX_MESSAGE_SIZE) {
				return Close(1009, "Message too large");
			}
			// Fast path, we received a whole frame and we don't need to combine it with anything
			ProcessDataFrame(header.opcode(), cur_char_pos, payload_len);
		} else {
			if (IsBuildingFrames()) {
				if (header.opcode() != 0) {
					return Close(1002, "Expected continuation frame");
				}
			} else {
				if (header.opcode() == 0) {
					return Close(1002, "Unexpected continuation frame");
				}
				m_frame_opcode = header.opcode();
			}

			if (m_frame_buffer.size() + payload_len > MAX_MESSAGE_SIZE) {
				return Close(1009, "Message too large");
			}

			m_frame_buffer.insert(m_frame_buffer.end(), cur_char_pos, cur_char_pos + payload_len);

			if (header.fin()) {
				// Assemble frame
				ProcessDataFrame(m_frame_opcode, m_frame_buffer.data(), m_frame_buffer.size());
				m_frame_opcode = 0;
				m_frame_buffer.clear();
			}
		}
		Consume((cur_char_pos - buffer.data()) + payload_len);
	}
}

void WebSocket::ProcessDataFrame(uint8_t opcode, char* buf, uint64_t len) {
	switch (opcode) {
	case 9: // Ping
		if (m_is_closing) return;
		Send(std::string_view(buf, len), 10); // Send Pong
		break;
	case 10: // Pong
		break;
	case 8: // Close
		if (m_is_closing) {
			OnClose();
		} else {
			if (len == 1) {
				Close(1002, "Incomplete close code");
				return;
			} else if (len >= 2) {
				bool invalid = false;
				uint16_t code = (uint8_t(buf[0]) << 8) | uint8_t(buf[1]);

				if (code < 1000 || code >= 5000) {
					invalid = true;
				}

				switch (code) {
				case 1004: case 1005: case 1006: case 1015:
					invalid = true;
				default:;
				}

				if (invalid) {
					Close(1002, "Invalid close code");
					return;
				}

				if (len > 2 && !IsValidUTF8(buf + 2, len - 2)) {
					Close(1002, "Close reason is not UTF-8");
					return;
				}
			}

			m_is_closing = true;

			// Send back a close message
			Send(std::string_view((char*)buf, len), 8);

			OnClose();
		}
		break;
	case 1: case 2: // Text or Binary
		if (m_is_closing) return;
		if (opcode == 1 && !IsValidUTF8(buf, len)) {
			return Close(1007, "Invalid UTF-8 in text frame");
		}
		OnMessage(std::string_view(buf, len));
		break;
	default:
		return Close(1002, "Unknown op code");
	}
}

void WebSocket::Close(uint16_t code, const char* reason) {
	if (m_is_closing) return;
	m_is_closing = true;

	if (code > 1000 && code <= 1015) {
		if (strcmp(reason, "") == 0)
			OnWarning(std::string("WS: Status code: ").append(std::to_string(code)));
		else
			OnWarning(std::string("WS: ")
					.append("(code " + std::to_string(code) + ") ").append(reason));
	}

	char coded[2];
	coded[0] = (code >> 8) & 0xFF;
	coded[1] = (code >> 0) & 0xFF;
	Send(std::string(coded, sizeof(coded)) + reason, 8);

	OnClose();
}
