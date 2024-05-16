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

#ifndef EP_MULTIPLAYER_CONNECTION_H
#define EP_MULTIPLAYER_CONNECTION_H

#include <queue>
#include <map>
#include <functional>
#include <type_traits>

#include "packet.h"
#include "../output.h"

namespace Multiplayer {

class Connection {
public:
	static void ParseAddress(std::string address, std::string& host, uint16_t& port);

	Connection() {}
	Connection(const Connection&) = delete;
	Connection(Connection&&) = default;
	Connection& operator=(const Connection&) = delete;
	Connection& operator=(Connection&&) = default;

	virtual ~Connection() = default;

	void SendPacket(const Packet& p);

	template<typename M, typename = std::enable_if_t<std::conjunction_v<
		std::is_convertible<M, Packet>,
		std::is_constructible<M, std::istream&>
	>>>
	void RegisterHandler(std::function<void (M&)> h) {
		handlers.emplace(M::packet_type, [this, h](std::istream& is) {
			M pack {is};
			std::invoke(h, pack);
		});
	}

	enum class SystemMessage {
		OPEN,
		CLOSE,
		TERMINATED, // client connection has terminated
		EOD, // end of data to flush packets
		_PLACEHOLDER,
	};
	using SystemMessageHandler = std::function<void (Connection&)>;
	void RegisterSystemHandler(SystemMessage m, SystemMessageHandler h);

protected:
	virtual void Open() = 0;
	virtual void Close() = 0;
	virtual void Send(std::string_view data) = 0;

	void Dispatch(const std::string_view data);
	void DispatchSystem(SystemMessage m);

	void Print(std::string_view tag, std::string_view data);

private:
	std::map<uint8_t, std::function<void (std::istream&)>> handlers;
	SystemMessageHandler sys_handlers[static_cast<size_t>(SystemMessage::_PLACEHOLDER)];
};

}

#endif
