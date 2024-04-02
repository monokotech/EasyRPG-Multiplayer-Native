#include <vector>
#include <functional>

// Defaults
enum class SOCKS5_DEFAULTS : std::uint8_t {
	RSV = 0x00,
	SUPPORT_AUTH = 0x01,
	VERSION = 0x05,
	VER_USERPASS = 0x01
};

// Currently supported AUTH Types (0x00 is default)
enum class SOCKS5_AUTH_TYPES : std::uint8_t {
	NOAUTH = 0x00,
	USERPASS = 0x02
};

// DNS resolve locally & make resolution on SOCKS5's endpoint
enum class SOCKS5_RESOLVE {
	REMOTE_RESOLVE = 0x01,
	LOCAL_RESOLVE = 0x02
};

enum class SOCKS5_ADDR_TYPE : std::uint8_t {
	IPv4 = 0x01,
	DOMAIN = 0x03,
	IPv6 = 0x04
};

// SOCKS5 Client connection request commands
enum class SOCKS5_CLIENT_CONNCMD : std::uint8_t {
	TCP_IP_STREAM = 0x01,
	TCP_IP_PORT_BIND = 0x02,
	UDP_PORT = 0x03
};

struct SOCKS5_Initializer {
	std::function<int(void*, const size_t&)> read;
	std::function<int(const void*, const size_t&)> write;

	int SendGreeting() const {
		std::vector<char> client_greeting_msg = {
			static_cast<char>(SOCKS5_DEFAULTS::VERSION),
			static_cast<char>(SOCKS5_DEFAULTS::SUPPORT_AUTH),
			static_cast<char>(SOCKS5_AUTH_TYPES::NOAUTH)
		};
		write(client_greeting_msg.data(), client_greeting_msg.size());

		std::vector<char> server_choice(2);
		int read_ret = read(server_choice.data(), server_choice.size());
		if(server_choice.at(0) == 0x05 && server_choice.at(1) == static_cast<char>(SOCKS5_AUTH_TYPES::NOAUTH)) {
			return 0;
		} else {
			return -1;
		}
	}

	int SendConnectionRequest(const std::string& destination_addr, const std::uint16_t& destination_port) {
		std::vector<char> client_conn_request = {
			static_cast<char>(SOCKS5_DEFAULTS::VERSION),
			static_cast<char>(SOCKS5_CLIENT_CONNCMD::TCP_IP_STREAM),
			static_cast<char>(SOCKS5_DEFAULTS::RSV),
			static_cast<char>(SOCKS5_ADDR_TYPE::DOMAIN),
			static_cast<char>(destination_addr.size())
		};
		for(std::size_t i{}; i < destination_addr.length(); i++)
			client_conn_request.push_back(destination_addr.at(i));
		client_conn_request.push_back(static_cast<char>(destination_port >> 8));
		client_conn_request.push_back(static_cast<char>(destination_port));
		write(client_conn_request.data(), client_conn_request.size());

		std::vector<char> server_responce(client_conn_request.size());
		int read_ret = read(server_responce.data(), server_responce.size());
		if(server_responce.at(0) == 0x05 && server_responce.at(1) == 0x00) {
			return 0;
		} else {
			return -1;
		}
	}
};

//int SOCKS5_AUTH::client_auth_handler() const {
//	std::vector<char> client_auth_request = {
//		static_cast<char>(SOCKS5_DEFAULTS::VER_USERPASS),
//		static_cast<char>(this->_proxy_username.length())
//	};
//	for(std::size_t i{}; i < this->_proxy_username.length(); i++){
//		client_auth_request.push_back(this->_proxy_username.at(i));
//	}
//
//	client_auth_request.push_back(static_cast<char>(this->_proxy_password.length()));
//	for(std::size_t i{}; i < this->_proxy_password.length(); i++){
//		client_auth_request.push_back(this->_proxy_password.at(i));
//	}
//	int write_ret = SOCKS5::write_data(this->_client_net_fd, client_auth_request.data(), client_auth_request.size(), 0);
//	std::vector<char> server_auth_responce(2);
//
//	int read_ret = SOCKS5::read_data(this->_client_net_fd, server_auth_responce.data(), server_auth_responce.size(), 0);
//	if(server_auth_responce.at(0) == static_cast<char>(SOCKS5_DEFAULTS::VER_USERPASS) && server_auth_responce.at(1) == 0x00){
//		return 0;
//	}else{
//		return -1;
//	}
//}
