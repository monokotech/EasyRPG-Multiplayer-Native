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

#include "server.h"
#include "socket.h"
#include <thread>
#include "../utils.h"
#include "output_mt.h"
#include "util/strfnd.h"

#ifdef SERVER
#  include <csignal>
#  include <getopt.h>
#  include <fstream>
#  include <ostream>
#  include <istream>
#  include <lcf/inireader.h>
#endif

using namespace Multiplayer;
using namespace Messages;

class ServerConnection : public Connection {
	std::unique_ptr<Socket> socket;

	void HandleData(std::string_view data) {
		Print("Server Received: ", data);
		Dispatch(data);
		DispatchSystem(SystemMessage::EOD);
	}

	void HandleOpen() {
		DispatchSystem(SystemMessage::OPEN);
	}

	void HandleClose() {
		DispatchSystem(SystemMessage::CLOSE);
	}

public:
	ServerConnection(std::unique_ptr<Socket>& _socket) {
		socket = std::move(_socket);
	}

	void SetReadTimeout(uint16_t read_timeout_ms) {
		socket->SetReadTimeout(read_timeout_ms);
	}

	void Open() override {
		socket->OnInfo = [](std::string_view m) { OutputMt::Info("S: {}", m); };
		socket->OnWarning = [](std::string_view m) { OutputMt::Warning("S: {}", m); };
		socket->OnMessage = [this](auto p1) { HandleData(p1); };
		socket->OnOpen = [this]() { HandleOpen(); };
		socket->OnClose = [this]() { HandleClose(); };
		socket->Open();
	}

	void Close() override {
		socket->Close();
	}

	void Send(std::string_view data) override {
		socket->Send(data); // send back to oneself
		Print("Server Sent: ", data);
	}
};

/**
 * Clients
 */

class ServerSideClient {
	static constexpr size_t QUEUE_MAX_BULK_SIZE = 4096;

	struct LastState {
		MovePacket move;
		FacingPacket facing;
		SpeedPacket speed;
		SpritePacket sprite;
		RepeatingFlashPacket repeating_flash;
		HiddenPacket hidden;
		SystemPacket system;
		std::map<int, ShowPicturePacket> pictures;
	};

	ServerMain* server;

	bool join_sent = false;
	int id{0};
	ServerConnection connection;

	int room_id{0};
	int chat_crypt_key_hash{0};
	std::string name{""};
	LastState last;

	void SendSelfRoomInfoAsync() {
		server->ForEachClient([this](const ServerSideClient& other) {
			if (other.id == id || other.room_id != room_id)
				return;
			SendSelfAsync(JoinPacket(other.id));
			SendSelfAsync(other.last.move);
			if (other.last.facing.facing != 0)
				SendSelfAsync(other.last.facing);
			if (other.last.speed.speed != 0)
				SendSelfAsync(other.last.speed);
			if (other.name != "")
				SendSelfAsync(NamePacket(other.id, other.name));
			if (other.last.sprite.index != -1)
				SendSelfAsync(other.last.sprite);
			if (other.last.repeating_flash.IsAvailable())
				SendSelfAsync(other.last.repeating_flash);
			if (other.last.hidden.is_hidden)
				SendSelfAsync(other.last.hidden);
			if (other.last.system.name != "")
				SendSelfAsync(other.last.system);
			for (const auto& it : other.last.pictures) {
				SendSelfAsync(it.second);
			}
		});
	}

	void InitConnection() {
		using SystemMessage = Connection::SystemMessage;

		connection.RegisterHandler<HeartbeatPacket>([this](HeartbeatPacket& p) {
			SendSelfAsync(p);
		});

		auto Leave = [this]() {
			SendLocalAsync(LeavePacket(id));
			FlushQueue();
		};

		connection.RegisterSystemHandler(SystemMessage::OPEN, [this](Connection& _) {
		});
		connection.RegisterSystemHandler(SystemMessage::CLOSE, [this, Leave](Connection& _) {
			if (join_sent) {
				Leave();
				SendGlobalChat(ChatPacket(id, 0, CV_GLOBAL, room_id, "", "*** id:"+
					std::to_string(id) + (name == "" ? "" : " " + name) + " left the server."));
				OutputMt::Info("S: room_id={} name={} left the server", room_id, name);
			}
			server->DeleteClient(id);
		});

		connection.RegisterHandler<RoomPacket>([this, Leave](RoomPacket& p) {
			// Some maps won't restore their actions, reset all here
			last.repeating_flash.Discard();
			last.pictures.clear();
			Leave();
			room_id = p.room_id;
			SendSelfAsync(p);
			SendSelfRoomInfoAsync();
			SendLocalAsync(JoinPacket(id));
			SendLocalAsync(last.move);
			SendLocalAsync(last.facing);
			SendLocalAsync(last.sprite);
			if (name != "")
				SendLocalAsync(NamePacket(id, name));
			if (last.system.name != "")
				SendLocalAsync(last.system);
		});
		connection.RegisterHandler<NamePacket>([this](NamePacket& p) {
			name = std::move(p.name);
			if (!join_sent) {
				SendGlobalChat(ChatPacket(id, 0, CV_GLOBAL, room_id, "", "*** id:"+
					std::to_string(id) + (name == "" ? "" : " " + name) + " joined the server."));
				OutputMt::Info("S: room_id={} name={} joined the server", room_id, name);

				SendSelfAsync(ConfigPacket(0, server->GetConfig().server_picture_names.Get()));
				SendSelfAsync(ConfigPacket(1, server->GetConfig().server_picture_prefixes.Get()));
				SendSelfAsync(ConfigPacket(2, server->GetConfig().server_virtual_3d_maps.Get()));

				join_sent = true;
			}
		});
		connection.RegisterHandler<ChatPacket>([this](ChatPacket& p) {
			p.id = id;
			p.type = 1; // 1 = chat
			p.room_id = room_id;
			p.name = name == "" ? "<unknown>" : name;
			p.sys_name = last.system.name;
			VisibilityType visibility = static_cast<VisibilityType>(p.visibility);
			if (visibility == CV_LOCAL) {
				SendLocalChat(p);
				OutputMt::Info("S: Chat: {} [LOCAL, {}]: {}", p.name, p.room_id, p.message);
			} else if (visibility == CV_GLOBAL) {
				SendGlobalChat(p);
				OutputMt::Info("S: Chat: {} [GLOBAL, {}]: {}", p.name, p.room_id, p.message);
			} else if (visibility == CV_CRYPT) {
				// use "crypt_key_hash != 0" to distinguish whether to set or send
				if (p.crypt_key_hash != 0) { // set
					// the chat_crypt_key_hash is used for searching
					chat_crypt_key_hash = p.crypt_key_hash;
					OutputMt::Info("S: Chat: {} [CRYPT, {}]: Update chat_crypt_key_hash: {}",
						p.name, p.room_id, chat_crypt_key_hash);
				} else { // send
					SendCryptChat(p);
				}
			}
		});
		connection.RegisterHandler<TeleportPacket>([this](TeleportPacket& p) {
			last.move.x = p.x;
			last.move.y = p.y;
		});
		connection.RegisterHandler<MovePacket>([this](MovePacket& p) {
			p.id = id;
			last.move = p;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<JumpPacket>([this](JumpPacket& p) {
			p.id = id;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<FacingPacket>([this](FacingPacket& p) {
			p.id = id;
			last.facing = p;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<SpeedPacket>([this](SpeedPacket& p) {
			p.id = id;
			last.speed = p;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<SpritePacket>([this](SpritePacket& p) {
			p.id = id;
			last.sprite = p;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<FlashPacket>([this](FlashPacket& p) {
			p.id = id;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<RepeatingFlashPacket>([this](RepeatingFlashPacket& p) {
			p.id = id;
			last.repeating_flash = p;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<RemoveRepeatingFlashPacket>([this](RemoveRepeatingFlashPacket& p) {
			p.id = id;
			last.repeating_flash.Discard();
			SendLocalAsync(p);
		});
		connection.RegisterHandler<HiddenPacket>([this](HiddenPacket& p) {
			p.id = id;
			last.hidden = p;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<SystemPacket>([this](SystemPacket& p) {
			p.id = id;
			last.system = p;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<SoundEffectPacket>([this](SoundEffectPacket& p) {
			p.id = id;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<ShowPicturePacket>([this](ShowPicturePacket& p) {
			p.id = id;
			if (last.pictures.size() < 200)
				last.pictures[p.pic_id] = p;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<MovePicturePacket>([this](MovePicturePacket& p) {
			p.id = id;
			const auto& it = last.pictures.find(p.pic_id);
			if(it != last.pictures.end()) {
				PicturePacket& pic = it->second;
				pic.params = p.params;
				pic = p;
			}
			SendLocalAsync(p);
		});
		connection.RegisterHandler<ErasePicturePacket>([this](ErasePicturePacket& p) {
			p.id = id;
			last.pictures.erase(p.pic_id);
			SendLocalAsync(p);
		});
		connection.RegisterHandler<ShowPlayerBattleAnimPacket>([this](ShowPlayerBattleAnimPacket& p) {
			p.id = id;
			SendLocalAsync(p);
		});

		connection.RegisterSystemHandler(SystemMessage::EOD, [this](Connection& _) {
			FlushQueue();
		});
	}

	/**
	 * Sending without queue
	 *  These will be sent back to oneself
	 */

	template<typename T>
	void SendLocalChat(const T& p) {
		server->SendTo(id, room_id, CV_LOCAL, p.ToBytes(), true);
	}

	template<typename T>
	void SendGlobalChat(const T& p) {
		server->SendTo(id, 0, CV_GLOBAL, p.ToBytes(), true);
	}

	template<typename T>
	void SendCryptChat(const T& p) {
		server->SendTo(id, 0, CV_CRYPT, p.ToBytes(), true);
	}

	/**
	 * Queue sending
	 */

	std::queue<std::unique_ptr<Packet>> m_self_queue;
	std::queue<std::unique_ptr<Packet>> m_local_queue;
	std::queue<std::unique_ptr<Packet>> m_global_queue;

	template<typename T>
	void SendPacketAsync(std::queue<std::unique_ptr<Packet>>& queue,
			const T& _p) {
		auto p = new T;
		*p = _p;
		queue.emplace(p);
	}

	template<typename T>
	void SendSelfAsync(const T& p) {
		SendPacketAsync(m_self_queue, p);
	}

	template<typename T>
	void SendLocalAsync(const T& p) {
		SendPacketAsync(m_local_queue, p);
	}

	template<typename T>
	void SendGlobalAsync(const T& p) {
		SendPacketAsync(m_global_queue, p);
	}

	void FlushQueueSend(const std::string& bulk,
			const VisibilityType& visibility, bool to_self) {
		if (to_self) {
			connection.Send(bulk);
		} else {
			int to_id = 0;
			if (visibility == Messages::CV_LOCAL) {
				to_id = room_id;
			}
			server->SendTo(id, to_id, visibility, bulk);
		}
	}

	void FlushQueue(std::queue<std::unique_ptr<Packet>>& queue,
			const VisibilityType& visibility, bool to_self = false) {
		std::string bulk;
		while (!queue.empty()) {
			const auto& e = queue.front();
			std::string data = e->ToBytes();
			if (bulk.size() + data.size() > QUEUE_MAX_BULK_SIZE) {
				FlushQueueSend(bulk, visibility, to_self);
				bulk.clear();
			}
			bulk += data;
			queue.pop();
		}
		if (!bulk.empty()) {
			FlushQueueSend(bulk, visibility, to_self);
		}
	}

	void FlushQueue() {
		FlushQueue(m_global_queue, CV_GLOBAL);
		FlushQueue(m_local_queue, CV_LOCAL);
		FlushQueue(m_self_queue, CV_NULL, true);
	}

public:
	ServerSideClient(ServerMain* _server, int _id, std::unique_ptr<Socket> _socket)
			: server(_server), id(_id), connection(ServerConnection(_socket)) {
		InitConnection();
	}

	void Open() {
		connection.SetReadTimeout(server->GetConfig().no_heartbeats.Get() ? 0 : 6000);
		connection.Open();
	}

	void Close() {
		connection.Close();
	}

	void Send(std::string_view data) {
		connection.Send(data);
	}

	int GetId() const {
		return id;
	}

	int GetRoomId() const {
		return room_id;
	}

	int GetChatCryptKeyHash() const {
		return chat_crypt_key_hash;
	}
};

/**
 * Server
 */

struct ServerMain::DataToSend {
	int from_id;
	int to_id;
	VisibilityType visibility;
	std::string data;
	bool return_flag;
};

void ServerMain::ForEachClient(const std::function<void(ServerSideClient&)>& callback) {
	if (!running) return;
	std::lock_guard lock(m_mutex);
	for (const auto& it : clients) {
		callback(*it.second);
	}
}

void ServerMain::DeleteClient(const int id) {
	std::lock_guard lock(m_mutex);
	clients.erase(id);
}

void ServerMain::SendTo(const int from_id, const int to_id,
		const VisibilityType visibility, std::string_view data,
		const bool return_flag) {
	if (!running) return;
	auto data_to_send = new DataToSend{ from_id, to_id, visibility,
			std::string(data), return_flag };
	{
		std::lock_guard lock(m_mutex);
		m_data_to_send_queue.emplace(data_to_send);
		m_data_to_send_queue_cv.notify_one();
	}
}

void ServerMain::Start(bool wait_thread) {
	if (running) return;
	running = true;

	std::thread([this]() {
		while (running) {
			std::unique_lock<std::mutex> lock(m_mutex);
			m_data_to_send_queue_cv.wait(lock, [this] {
					return !m_data_to_send_queue.empty(); });
			auto& data_to_send = m_data_to_send_queue.front();
			// stop the thread
			if (data_to_send->from_id == 0 &&
					data_to_send->visibility == Messages::CV_NULL) {
				m_data_to_send_queue.pop();
				break;
			}
			// check if the client is online
			ServerSideClient* from_client = nullptr;
			const auto& from_client_it = clients.find(data_to_send->from_id);
			if (from_client_it != clients.end()) {
				from_client = from_client_it->second.get();
			}
			// send to global, local and crypt
			if (data_to_send->visibility == Messages::CV_LOCAL ||
				data_to_send->visibility == Messages::CV_CRYPT ||
					data_to_send->visibility == Messages::CV_GLOBAL) {
				// enter on every client
				for (const auto& it : clients) {
					auto& to_client = it.second;
					// exclude self
					if (!data_to_send->return_flag &&
							data_to_send->from_id == to_client->GetId())
						continue;
					// send to local
					if (data_to_send->visibility == Messages::CV_LOCAL &&
							data_to_send->to_id == to_client->GetRoomId()) {
						to_client->Send(data_to_send->data);
					// send to crypt
					} else if (data_to_send->visibility == Messages::CV_CRYPT &&
							from_client && from_client->GetChatCryptKeyHash() == to_client->GetChatCryptKeyHash()) {
						to_client->Send(data_to_send->data);
					// send to global
					} else if (data_to_send->visibility == Messages::CV_GLOBAL) {
						to_client->Send(data_to_send->data);
					}
				}
			}
			m_data_to_send_queue.pop();
		}
	}).detach();

	auto CreateServerSideClient = [this](std::unique_ptr<Socket> socket) {
		if (clients.size() >= cfg.server_max_users.Get()) {
			std::string_view data = "\uFFFD1";
			socket->Send(data);
			socket->Close();
		} else {
			auto& client = clients[client_id];
			client.reset(new ServerSideClient(this, client_id++, std::move(socket)));
			client->Open();
		}
	};

	if (cfg.server_bind_address_2.Get() != "") {
		server_listener_2.reset(new ServerListener(addr_host_2, addr_port_2));
		server_listener_2->OnInfo = [](std::string_view m) { OutputMt::Info("S: {}", m); };
		server_listener_2->OnWarning = [](std::string_view m) { OutputMt::Warning("S: {}", m); };
		server_listener_2->OnConnection = CreateServerSideClient;
		server_listener_2->Start();
	}

	server_listener.reset(new ServerListener(addr_host, addr_port));
	server_listener->OnInfo = [](std::string_view m) { OutputMt::Info("S: {}", m); };
	server_listener->OnWarning = [](std::string_view m) { OutputMt::Warning("S: {}", m); };
	server_listener->OnConnection = CreateServerSideClient;
	server_listener->Start(wait_thread);
}

void ServerMain::Stop() {
	if (!running) return;
	Output::Debug("Server: Stopping");
	std::lock_guard lock(m_mutex);
	running = false;
	for (const auto& it : clients) {
		it.second->Send("\uFFFD0");
		// the client will be removed upon SystemMessage::CLOSE
		it.second->Close();
	}
	if (server_listener_2)
		server_listener_2->Stop();
	server_listener->Stop();
	// stop sending loop
	m_data_to_send_queue.emplace(new DataToSend{ 0, 0, Messages::CV_NULL, "" });
	m_data_to_send_queue_cv.notify_one();
	Output::Info("S: Stopped");
}

void ServerMain::SetConfig(const Game_ConfigMultiplayer& _cfg) {
	cfg = _cfg;
	Connection::ParseAddress(cfg.server_bind_address.Get(), addr_host, addr_port);
	if (cfg.server_bind_address_2.Get() != "")
		Connection::ParseAddress(cfg.server_bind_address_2.Get(), addr_host_2, addr_port_2);
}

Game_ConfigMultiplayer ServerMain::GetConfig() const {
	return cfg;
}

static ServerMain _instance;

ServerMain& Server() {
	return _instance;
}

#ifdef SERVER
int main(int argc, char *argv[])
{
	Game_ConfigMultiplayer cfg;
	std::string config_path{""};

	const char* short_opts = "a:A:nc:";
	const option long_opts[] = {
		{"bind-address", required_argument, nullptr, 'a'},
		{"bind-address-2", required_argument, nullptr, 'A'},
		{"no-heartbeats", no_argument, nullptr, 'n'},
		{"config-path", required_argument, nullptr, 'c'},
		{nullptr, no_argument, nullptr, 0}
	};

	while (true) {
		const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);
		if (opt == 'a')
			cfg.server_bind_address.Set(optarg);
		else if (opt == 'A')
			cfg.server_bind_address_2.Set(optarg);
		else if (opt == 'n')
			cfg.no_heartbeats.Set(true);
		else if (opt == 'c')
			config_path = optarg;
		else
			break;
	}

	if (config_path != "") {
		// create file if not exists
		{ std::ofstream ofs(config_path, std::ios::app); }
		std::ifstream ifs(config_path);
		std::istream& is = ifs;
		lcf::INIReader ini(is);
		cfg.server_bind_address.FromIni(ini);
		cfg.server_bind_address_2.FromIni(ini);
		cfg.server_max_users.FromIni(ini);
		cfg.server_picture_names.FromIni(ini);
		cfg.server_picture_prefixes.FromIni(ini);
		cfg.server_virtual_3d_maps.FromIni(ini);
	}

	Server().SetConfig(cfg);

	auto signal_handler = [](int signal) {
		Server().Stop();
		Output::Debug("Server: signal={}", signal);
	};
	std::signal(SIGINT, signal_handler);
	std::signal(SIGTERM, signal_handler);

	Server().Start(true);

	return EXIT_SUCCESS;
}
#endif
