#ifndef EP_SOCKS5_H
#define EP_SOCKS5_H

#include <vector>

// Defaults
enum class SOCKS5_DEFAULTS : std::uint8_t {
	SS_RSV = 0x00,
	SS_SUPPORT_AUTH = 0x01,
	SS_VERSION = 0x05,
	SS_VER_USERPASS = 0x01
};

// Currently supported AUTH Types (0x00 is default)
enum class SOCKS5_AUTH_TYPES : std::uint8_t {
	SS_NOAUTH = 0x00,
	SS_USERPASS = 0x02
};

// DNS resolve locally & make resolution on SOCKS5's endpoint
enum class SOCKS5_RESOLVE {
	SS_REMOTE_RESOLVE = 0x01,
	SS_LOCAL_RESOLVE = 0x02
};

enum class SOCKS5_ADDR_TYPE : std::uint8_t {
	SS_IPv4 = 0x01,
	SS_DOMAIN = 0x03,
	SS_IPv6 = 0x04
};

// SOCKS5 Client connection request commands
enum class SOCKS5_CLIENT_CONNCMD : std::uint8_t {
	SS_TCP_IP_STREAM = 0x01,
	SS_TCP_IP_PORT_BIND = 0x02,
	SS_UDP_PORT = 0x03
};

struct SOCKS5 {
	static const std::vector<char> GetGreeting() {
		std::vector<char> client_greeting_msg = {
			static_cast<char>(SOCKS5_DEFAULTS::SS_VERSION),
			static_cast<char>(SOCKS5_DEFAULTS::SS_SUPPORT_AUTH),
			static_cast<char>(SOCKS5_AUTH_TYPES::SS_NOAUTH)
		};
		return std::move(client_greeting_msg);
	}

	static int CheckGreeting(const std::vector<char>& server_choice) {
		if(server_choice.size() == 2 && server_choice.at(0) == 0x05 &&
				server_choice.at(1) == static_cast<char>(SOCKS5_AUTH_TYPES::SS_NOAUTH)) {
			return 0;
		} else {
			return -1;
		}
	}

	static const std::vector<char> GetConnectionRequest(const std::string_view destination_addr,
			const uint16_t destination_port) {
		std::vector<char> client_conn_request = {
			static_cast<char>(SOCKS5_DEFAULTS::SS_VERSION),
			static_cast<char>(SOCKS5_CLIENT_CONNCMD::SS_TCP_IP_STREAM),
			static_cast<char>(SOCKS5_DEFAULTS::SS_RSV),
			static_cast<char>(SOCKS5_ADDR_TYPE::SS_DOMAIN),
			static_cast<char>(destination_addr.size())
		};
		for(std::size_t i{}; i < destination_addr.length(); i++)
			client_conn_request.push_back(destination_addr.at(i));
		client_conn_request.push_back(static_cast<char>(destination_port >> 8));
		client_conn_request.push_back(static_cast<char>(destination_port));
		return std::move(client_conn_request);
	}

	static int CheckConnectionRequest(const std::vector<char>& server_responce) {
		if(server_responce.size() >= 2 &&
				server_responce.at(0) == 0x05 && server_responce.at(1) == 0x00) {
			return 0;
		} else {
			return -1;
		}
	}
};

#endif
