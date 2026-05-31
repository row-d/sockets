
#pragma once

#include <cstdint>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace net::socket
{
#ifdef _WIN32
	using SocketHandle = SOCKET;
	constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
	using SocketHandle = int;
	constexpr SocketHandle kInvalidSocket = -1;
#endif

	class SocketInit
	{
	public:
		SocketInit();
		~SocketInit();

		SocketInit(const SocketInit &) = delete;
		SocketInit &operator=(const SocketInit &) = delete;
	};

	bool set_non_blocking(SocketHandle handle, bool enabled, std::string *error);
	SocketHandle create_tcp_listener(uint16_t port, std::string *error);
	SocketHandle accept_client(SocketHandle listener, std::string *error);
	int recv_data(SocketHandle handle, char *buffer, int length, std::string *error);
	int send_data(SocketHandle handle, const char *buffer, int length, std::string *error);
	void close_socket(SocketHandle handle);
	int get_last_error();
	bool would_block(int error_code);
}