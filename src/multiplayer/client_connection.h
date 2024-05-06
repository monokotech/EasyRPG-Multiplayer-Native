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

#ifndef EP_CLIENTCONNECTION_H
#define EP_CLIENTCONNECTION_H

#include <mutex>
#include "connection.h"
#include "../game_config.h"

class ConnectorSocket;

using namespace Multiplayer;

class ClientConnection : public Connection {
public:
	void SetConfig(Game_ConfigMultiplayer* _cfg) {
		cfg = _cfg;
	}

	void SetAddress(std::string_view address);
	void SetSocks5Address(std::string_view address);

	ClientConnection();
	ClientConnection(ClientConnection&&);
	ClientConnection& operator=(ClientConnection&&);
	~ClientConnection();

	bool IsConnected() const { return connected; }

	void Open() override;
	void Close() override;
	void Send(std::string_view data) override;

	template<typename T, typename... Args>
	void SendPacketAsync(Args... args) {
		if (connected) {
			m_queue.emplace(new T(args...));
		}
	}

	void Receive();
	void FlushQueue();

protected:
	Game_ConfigMultiplayer* cfg;

	std::string addr_host;
	uint16_t addr_port{ 6500 };
	std::string socks5_addr_host;
	uint16_t socks5_addr_port{ 1080 };

	std::unique_ptr<ConnectorSocket> socket;

	bool connecting = false;
	bool connected = false;

	std::queue<SystemMessage> m_system_queue;
	std::queue<std::string> m_data_queue;
	std::mutex m_receive_mutex;

	std::queue<std::unique_ptr<Packet>> m_queue;

	void HandleOpen();
	void HandleCloseOrTerm(bool terminated = false);
	void HandleData(std::string_view);
};

#endif
