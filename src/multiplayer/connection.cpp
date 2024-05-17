/*
 * EPMP
 * See: docs/LICENSE-EPMP.txt
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

#include "connection.h"
#include "util/hexdump.h"

using namespace Multiplayer;

void Connection::ParseAddress(std::string address, std::string& host, uint16_t& port) {
	size_t pos = 0;
	if (address.find("[") != std::string::npos) {
		address.erase(0, 1);
		pos = address.find("]:");
		if (pos == std::string::npos) {
			address.erase(address.size() - 1);
			host = address;
			return;
		}
		host = address.substr(0, pos);
		address.erase(0, pos + 2);
		port = atoi(address.c_str());
		return;
	}
	pos = address.find(":");
	if (pos == std::string::npos) {
		host = address;
		return;
	}
	host = address.substr(0, pos);
	address.erase(0, pos + 1);
	port = atoi(address.c_str());
}

void Connection::SendPacket(const Packet& p) {
	Send(p.ToBytes());
}

void Connection::Dispatch(const std::string_view data) {
	std::istringstream iss(std::string(data), std::ios_base::binary);
	while (!iss.eof()) {
		std::istringstream pkt_iss(DeSerializeString16(iss));
		auto packet_type = ReadU8(pkt_iss);
		auto it = handlers.find(packet_type);
		if (it != handlers.end()) {
			std::invoke(it->second, pkt_iss);
		} else {
			Output::Debug("Connection: Unregistered packet received");
			break;
		}
		ReadU16(pkt_iss); // skip unused bytes
		iss.peek(); // check eof
	}
}

void Connection::RegisterSystemHandler(SystemMessage m, SystemMessageHandler h) {
	sys_handlers[static_cast<size_t>(m)] = h;
}

void Connection::DispatchSystem(SystemMessage m) {
	auto f = sys_handlers[static_cast<size_t>(m)];
	if (f)
		std::invoke(f, *this);
}

void Connection::Print(std::string_view tag, std::string_view data) {
	if (data == std::string("\x00\x03\x01\x28\x28", 5)) { // heartbeat
		return;
	}
	Output::Debug("{}{} bytes\n{}", tag, data.size(), HexDump(data));
}
