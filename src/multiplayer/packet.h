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

#ifndef EP_MULTIPLAYER_PACKET_H
#define EP_MULTIPLAYER_PACKET_H

#include "util/serialize.h"

namespace Multiplayer {

class Packet {
public:
	Packet() {}
	Packet(uint8_t _packet_type) : packet_type(_packet_type) {}

	virtual ~Packet() = default;
	virtual std::string ToBytes() const;

	uint8_t GetType() const { return packet_type; }

protected:
	template<typename T>
	static void WritePartial(std::ostream& os, T val) {
		Write(os, val);
	}

	template<typename T, typename... Args>
	static void WritePartial(std::ostream& os, T val, Args... args) {
		Write(os, val);
		WritePartial(os, args...);
	}

private:
	static void Write(std::ostream& os, bool val) { WriteU8(os, val); }

	static void Write(std::ostream& os, uint8_t val) { WriteU8(os, val); }
	static void Write(std::ostream& os, uint16_t val) { WriteU16(os, val); }
	static void Write(std::ostream& os, uint32_t val) { WriteU32(os, val); }

	static void Write(std::ostream& os, int8_t val) { WriteS8(os, val); }
	static void Write(std::ostream& os, int16_t val) { WriteS16(os, val); }
	static void Write(std::ostream& os, int32_t val) { WriteS32(os, val); }

	static void Write(std::ostream& os, std::string_view val) { os << SerializeString16(val); }

	virtual void Serialize(std::ostream& os) const {}

	uint8_t packet_type{0};
};

}
#endif
