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

#ifndef EP_SERVER_H
#define EP_SERVER_H

#include <memory>
#include <map>
#include <queue>
#include <condition_variable>
#include <mutex>
#include "messages.h"
#include "../game_config.h"

class ServerListener;
class ServerSideClient;

class ServerMain {
	struct DataToSend;

	bool running = false;
	int client_id = 10;
	std::map<int, std::unique_ptr<ServerSideClient>> clients;

	std::unique_ptr<ServerListener> server_listener;
	std::unique_ptr<ServerListener> server_listener_2;

	std::string addr_host;
	std::string addr_host_2;
	uint16_t addr_port{ 6500 };
	uint16_t addr_port_2{ 6500 };

	std::queue<std::unique_ptr<DataToSend>> m_data_to_send_queue;
	std::condition_variable m_data_to_send_queue_cv;

	std::mutex m_mutex;

	Game_ConfigMultiplayer cfg;

public:
	void Start(bool wait_thread = false);
	void Stop();

	void SetConfig(const Game_ConfigMultiplayer& _cfg);
	Game_ConfigMultiplayer GetConfig() const;

	void ForEachClient(const std::function<void(ServerSideClient&)>& callback);
	void DeleteClient(const int& id);
	void SendTo(const int& from_client_id, const int& to_client_id,
		const Messages::VisibilityType& visibility, const std::string& data,
		const bool& return_flag = false);
};

ServerMain& Server();

#endif
