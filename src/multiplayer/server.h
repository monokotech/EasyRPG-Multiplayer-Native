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
	struct MessageDataEntry;

	bool running = false;
	int client_id = 10;
	std::map<int, std::unique_ptr<ServerSideClient>> clients;

	std::unique_ptr<ServerListener> server_listener;
	std::unique_ptr<ServerListener> server_listener_2;

	std::string addr_host;
	std::string addr_host_2;
	uint16_t addr_port{ 6500 };
	uint16_t addr_port_2{ 6500 };

	std::queue<std::unique_ptr<MessageDataEntry>> m_message_data_queue;
	std::condition_variable m_message_data_queue_cv;

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
