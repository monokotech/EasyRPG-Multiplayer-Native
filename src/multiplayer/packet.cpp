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

#include "packet.h"
#include <sstream>

using namespace Multiplayer;

std::string Packet::ToBytes() const {
	std::ostringstream oss(std::ios_base::binary);
	WritePartial(oss, GetType());
	Serialize(oss);
	// just to see boundaries from hexdump
	WritePartial(oss, (uint16_t)0x2828);
	return SerializeString16(oss.str());
}
