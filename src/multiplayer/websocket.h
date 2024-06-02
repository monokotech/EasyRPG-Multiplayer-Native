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

#ifndef EP_WEBSOCKET_H
#define EP_WEBSOCKET_H

#include <cstdint>
#include <string_view>
#include <functional>
#include <vector>

class WebSocket {
public:
	void Send(std::string_view data, uint8_t opcode = 2); // 2: binary
	std::function<void(std::string_view data)> OnWrite;

	void GotData(std::string_view data);
	std::function<void(std::string_view data)> OnMessage;

	void Close(uint16_t code = 1000, const char* reason = ""); // 1000: normal
	std::function<void()> OnClose;

	std::function<void(std::string_view data)> OnWarning;

private:
	enum : unsigned char {
		WS_NO_FRAMES = 0
	};

	bool m_has_completed_handshake = false;
	bool m_is_closing = false;

	std::vector<char> m_buffer;

	uint8_t m_frame_opcode = WS_NO_FRAMES;
	std::vector<char> m_frame_buffer;
	inline bool IsBuildingFrames() { return m_frame_opcode != WS_NO_FRAMES; }

	// Stub, maybe some day
	inline bool IsValidUTF8(const char *, size_t) { return true; }

	void ProcessDataFrame(uint8_t opcode, char* buf, size_t len);
};

#endif
