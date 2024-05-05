#include "server.h"
#include "socket.h"
#include <thread>
#include "../utils.h"
#include "../output.h"
#include "strfnd.h"

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

constexpr size_t MAX_BULK_SIZE = Connection::MAX_QUEUE_SIZE -
		Packet::MSG_DELIM.size();

class ServerConnection : public Connection {
	const int& id;
	ServerMain* server;

	std::unique_ptr<Socket> socket;

	std::queue<std::unique_ptr<Packet>> m_self_queue;
	std::queue<std::unique_ptr<Packet>> m_local_queue;
	std::queue<std::unique_ptr<Packet>> m_global_queue;

protected:
	void HandleData(std::string_view data) {
		DispatchMessages(data);
		DispatchSystem(SystemMessage::EOM);
	}

	void HandleOpen() {
		DispatchSystem(SystemMessage::OPEN);
	}

	void HandleClose() {
		DispatchSystem(SystemMessage::CLOSE);
		server->DeleteClient(id);
	}

public:
	ServerConnection(const int& _id, ServerMain* _server, std::unique_ptr<Socket>& _socket)
			: id(_id), server(_server) {
		socket = std::move(_socket);
	}

	void Open() override {
		socket->OnInfo = [](std::string_view m) { Output::Info(m); };
		socket->OnWarning = [](std::string_view m) { Output::Warning(m); };
		socket->OnData = [this](auto p1) { HandleData(p1); };
		socket->OnOpen = [this]() { HandleOpen(); };
		socket->OnClose = [this]() { HandleClose(); };
		socket->SetReadTimeout(server->GetConfig().no_heartbeats.Get() ? 0 : 6000);
		socket->Open();
	}

	void Close() override {
		socket->Close();
	}

	void Send(std::string_view data) override {
		socket->Send(data); // send to self
	}

	template<typename T>
	void SendPacketAsync(std::queue<std::unique_ptr<Packet>>& queue,
			const T& _p) {
		auto p = new T;
		*p = _p;
		queue.emplace(p);
	}

	template<typename T>
	void SendSelfPacketAsync(const T& p) {
		SendPacketAsync(m_self_queue, p);
	}

	template<typename T>
	void SendLocalPacketAsync(const T& p) {
		SendPacketAsync(m_local_queue, p);
	}

	template<typename T>
	void SendGlobalPacketAsync(const T& p) {
		SendPacketAsync(m_global_queue, p);
	}

	void FlushQueue(std::queue<std::unique_ptr<Packet>>& queue,
			const VisibilityType& visibility, bool self = false) {
		std::string bulk;
		while (!queue.empty()) {
			const auto& e = queue.front();
			std::string data = std::move(e->ToBytes());
			if (bulk.size() + data.size() > MAX_BULK_SIZE) {
				if (self)
					Send(bulk);
				else
					server->SendTo(id, 0, visibility, bulk);
				bulk.clear();
			}
			if (!bulk.empty())
				bulk += Packet::MSG_DELIM;
			bulk += data;
			queue.pop();
		}
		if (!bulk.empty()) {
			if (self)
				Send(bulk);
			else
				server->SendTo(id, 0, visibility, bulk);
		}
	}

	void FlushQueue() {
		FlushQueue(m_global_queue, CV_GLOBAL);
		FlushQueue(m_local_queue, CV_LOCAL);
		FlushQueue(m_self_queue, CV_NULL, true);
	}
};

/**
 * Clients
 */

struct ServerMain::DataToSend {
	int from_client_id;
	int to_client_id;
	VisibilityType visibility;
	std::string data;
	bool return_flag;
};

class ServerSideClient {
	struct LastState {
		MovePacket move;
		FacingPacket facing;
		SpeedPacket speed;
		SpritePacket sprite;
		RepeatingFlashPacket repeating_flash;
		HiddenPacket hidden;
		SystemPacket system;
		std::map<int, ShowPicturePacket> pictures;

		bool has_repeating_flash{ false };
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
			if (other.last.has_repeating_flash)
				SendSelfAsync(other.last.repeating_flash);
			if (other.last.hidden.hidden_bin == 1)
				SendSelfAsync(other.last.hidden);
			if (other.last.system.name != "")
				SendSelfAsync(other.last.system);
			for (const auto& it : other.last.pictures) {
				SendSelfAsync(it.second);
			}
		});
	}

	void InitConnection() {
		connection.RegisterHandler<HeartbeatPacket>([this](HeartbeatPacket& p) {
			SendSelfAsync(p);
		});

		using SystemMessage = Connection::SystemMessage;

		connection.RegisterSystemHandler(SystemMessage::OPEN, [this](Connection& _) {
		});
		connection.RegisterSystemHandler(SystemMessage::CLOSE, [this](Connection& _) {
			if (join_sent) {
				SendGlobal(LeavePacket(id));
				SendGlobalChat(ChatPacket(id, 0, CV_GLOBAL, room_id, "", "*** id:"+
					std::to_string(id) + (name == "" ? "" : " " + name) + " left the server."));
				Output::Info("Server: room_id={} name={} left the server", room_id, name);
			}
		});

		connection.RegisterHandler<RoomPacket>([this](RoomPacket& p) {
			last.pictures.clear();
			SendGlobalAsync(LeavePacket(id));
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
				Output::Info("Server: room_id={} name={} joined the server", room_id, name);

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
				Output::Info("Server: Chat: {} [LOCAL, {}]: {}", p.name, p.room_id, p.message);
			} else if (visibility == CV_GLOBAL) {
				SendGlobalChat(p);
				Output::Info("Server: Chat: {} [GLOBAL, {}]: {}", p.name, p.room_id, p.message);
			} else if (visibility == CV_CRYPT) {
				// use "crypt_key_hash != 0" to distinguish whether to set or send
				if (p.crypt_key_hash != 0) { // set
					// the chat_crypt_key_hash is used for searching
					chat_crypt_key_hash = p.crypt_key_hash;
					Output::Info("Server: Chat: {} [CRYPT, {}]: Update chat_crypt_key_hash: {}",
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
			last.has_repeating_flash = true;
			SendLocalAsync(p);
		});
		connection.RegisterHandler<RemoveRepeatingFlashPacket>([this](RemoveRepeatingFlashPacket& p) {
			p.id = id;
			last.has_repeating_flash = false;
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
		connection.RegisterHandler<SEPacket>([this](SEPacket& p) {
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

		connection.RegisterSystemHandler(SystemMessage::EOM, [this](Connection& _) {
			FlushQueue();
		});
	}

	template<typename T>
	void SendSelfAsync(const T& p) {
		connection.SendSelfPacketAsync(p);
	}

	template<typename T>
	void SendLocalAsync(const T& p) {
		connection.SendLocalPacketAsync(p);
	}

	template<typename T>
	void SendGlobalAsync(const T& p) {
		connection.SendGlobalPacketAsync(p);
	}

	void FlushQueue() {
		connection.FlushQueue();
	}

	// also send to self
	template<typename T>
	void SendLocalChat(const T& p) {
		server->SendTo(id, 0, CV_LOCAL, p.ToBytes(), true);
	}

	// also send to self
	template<typename T>
	void SendGlobalChat(const T& p) {
		server->SendTo(id, 0, CV_GLOBAL, p.ToBytes(), true);
	}

	// also send to self
	template<typename T>
	void SendCryptChat(const T& p) {
		server->SendTo(id, 0, CV_CRYPT, p.ToBytes(), true);
	}

	// for no EOM and non-async order
	template<typename T>
	void SendGlobal(const T& p) {
		server->SendTo(id, 0, CV_GLOBAL, p.ToBytes());
	}

public:
	ServerSideClient(ServerMain* _server, int _id, std::unique_ptr<Socket> _socket)
			: server(_server), id(_id),
			connection(ServerConnection(id, _server, _socket)) {
		InitConnection();
	}

	void Open() {
		connection.Open();
	}

	void Close() {
		connection.Close();
	}

	void Send(std::string_view data) {
		connection.Send(data);
	}

	const int& GetId() {
		return id;
	}

	const int& GetRoomId() {
		return room_id;
	}

	const int& GetChatCryptKeyHash() {
		return chat_crypt_key_hash;
	}
};

/**
 * Server
 */

void ServerMain::ForEachClient(const std::function<void(ServerSideClient&)>& callback) {
	if (!running) return;
	std::lock_guard lock(m_mutex);
	for (const auto& it : clients) {
		callback(*it.second);
	}
}

void ServerMain::DeleteClient(const int& id) {
	std::lock_guard lock(m_mutex);
	clients.erase(id);
}

void ServerMain::SendTo(const int& from_client_id, const int& to_client_id,
		const VisibilityType& visibility, const std::string& data,
		const bool& return_flag) {
	if (!running) return;
	auto data_to_send = new DataToSend{ from_client_id, to_client_id, visibility, data,
			return_flag };
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
			if (data_to_send->from_client_id == 0 &&
					data_to_send->visibility == Messages::CV_NULL) {
				m_data_to_send_queue.pop();
				break;
			}
			// check if the client is online
			ServerSideClient* from_client = nullptr;
			const auto& from_client_it = clients.find(data_to_send->from_client_id);
			if (from_client_it != clients.end()) {
				from_client = from_client_it->second.get();
			}
			// send to global, local and crypt
			if (data_to_send->to_client_id == 0) {
				// enter on every client
				for (const auto& it : clients) {
					auto& to_client = it.second;
					// exclude self
					if (!data_to_send->return_flag &&
							data_to_send->from_client_id == to_client->GetId())
						continue;
					// send to local
					if (data_to_send->visibility == Messages::CV_LOCAL &&
							from_client && from_client->GetRoomId() == to_client->GetRoomId()) {
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
		server_listener_2->OnInfo = [](std::string_view m) { Output::Info(m); };
		server_listener_2->OnWarning = [](std::string_view m) { Output::Warning(m); };
		server_listener_2->OnConnection = CreateServerSideClient;
		server_listener_2->Start();
	}

	server_listener.reset(new ServerListener(addr_host, addr_port));
	server_listener->OnInfo = [](std::string_view m) { Output::Info(m); };
	server_listener->OnWarning = [](std::string_view m) { Output::Warning(m); };
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
		// the client will be removed from HandleClose
		it.second->Close();
	}
	if (server_listener_2)
		server_listener_2->Stop();
	server_listener->Stop();
	// stop sending loop
	m_data_to_send_queue.emplace(new DataToSend{ 0, 0, Messages::CV_NULL, "" });
	m_data_to_send_queue_cv.notify_one();
	Output::Debug("Server: Stopped");
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
			cfg.server_bind_address.Set(std::string(optarg));
		else if (opt == 'A')
			cfg.server_bind_address_2.Set(std::string(optarg));
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
