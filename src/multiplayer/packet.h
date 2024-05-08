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

#include <string>
#include <charconv>
#include <stdexcept>

namespace Multiplayer {

class Packet {
public:
	constexpr static std::string_view PARAM_DELIM = "\uFFFF";
	constexpr static std::string_view MSG_DELIM = "\uFFFE";

	Packet() {}
	Packet(std::string_view _packet_name) : packet_name(_packet_name) {}

	virtual ~Packet() = default;
	virtual std::string ToBytes() const;

	std::string_view GetName() const { return packet_name; }

protected:
	static std::string Sanitize(std::string_view param);

	static std::string ToString(const char* x) { return ToString(std::string_view(x)); }
	static std::string ToString(int x) { return std::to_string(x); }
	static std::string ToString(bool x) { return x ? "1" : "0"; }
	static std::string ToString(std::string_view v) { return Sanitize(v); }

	template<typename... Args>
	std::string Build(Args... args) const {
		std::string r {packet_name};
		AppendPartial(r, args...);
		return r;
	}

	static void AppendPartial(std::string& s) {}

	template<typename T>
	static void AppendPartial(std::string& s, T t) {
		s += PARAM_DELIM;
		s += ToString(t);
	}

	template<typename T, typename... Args>
	static void AppendPartial(std::string& s, T t, Args... args) {
		s += PARAM_DELIM;
		s += ToString(t);
		AppendPartial(s, args...);
	}

	template<typename T>
	static T Decode(std::string_view s);

private:
	std::string packet_name{ "" };
};

}
#endif
