#include "socket.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace net::socket
{
	SocketInit::SocketInit()
	{
#ifdef _WIN32
		WSADATA data{};
		WSAStartup(MAKEWORD(2, 2), &data);
#endif
	}

	SocketInit::~SocketInit()
	{
#ifdef _WIN32
		WSACleanup();
#endif
	}

	bool set_non_blocking(SocketHandle handle, bool enabled, std::string *error)
	{
#ifdef _WIN32
		u_long mode = enabled ? 1UL : 0UL;
		if (ioctlsocket(handle, FIONBIO, &mode) != 0)
		{
			if (error)
			{
				*error = "ioctlsocket(FIONBIO) failed";
			}
			return false;
		}
		return true;
#else
		int flags = fcntl(handle, F_GETFL, 0);
		if (flags < 0)
		{
			if (error)
			{
				*error = "fcntl(F_GETFL) failed";
			}
			return false;
		}

		int updated = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
		if (fcntl(handle, F_SETFL, updated) != 0)
		{
			if (error)
			{
				*error = "fcntl(F_SETFL) failed";
			}
			return false;
		}

		return true;
#endif
	}

	SocketHandle create_tcp_listener(uint16_t port, std::string *error)
	{
		SocketHandle handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (handle == kInvalidSocket)
		{
			if (error)
			{
				*error = "socket() failed";
			}
			return kInvalidSocket;
		}

		int reuse = 1;
#ifdef _WIN32
		setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#else
		setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		address.sin_addr.s_addr = htonl(INADDR_ANY);

		if (::bind(handle, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
		{
			if (error)
			{
				*error = "bind() failed";
			}
			close_socket(handle);
			return kInvalidSocket;
		}

		if (::listen(handle, SOMAXCONN) != 0)
		{
			if (error)
			{
				*error = "listen() failed";
			}
			close_socket(handle);
			return kInvalidSocket;
		}

		if (!set_non_blocking(handle, true, error))
		{
			close_socket(handle);
			return kInvalidSocket;
		}

		return handle;
	}

	SocketHandle accept_client(SocketHandle listener, std::string *error)
	{
		sockaddr_in client_addr{};
#ifdef _WIN32
		int len = sizeof(client_addr);
		SocketHandle client = ::accept(listener, reinterpret_cast<sockaddr *>(&client_addr), &len);
#else
		socklen_t len = sizeof(client_addr);
		SocketHandle client = ::accept(listener, reinterpret_cast<sockaddr *>(&client_addr), &len);
#endif

		if (client == kInvalidSocket)
		{
			if (error)
			{
				*error = "accept() failed";
			}
			return kInvalidSocket;
		}

		if (!set_non_blocking(client, true, error))
		{
			close_socket(client);
			return kInvalidSocket;
		}

		return client;
	}

	int recv_data(SocketHandle handle, char *buffer, int length, std::string *error)
	{
		int received = ::recv(handle, buffer, length, 0);
		if (received < 0)
		{
			if (error)
			{
				*error = "recv() failed";
			}
		}
		return received;
	}

	int send_data(SocketHandle handle, const char *buffer, int length, std::string *error)
	{
		int sent = ::send(handle, buffer, length, 0);
		if (sent < 0)
		{
			if (error)
			{
				*error = "send() failed";
			}
		}
		return sent;
	}

	void close_socket(SocketHandle handle)
	{
		if (handle == kInvalidSocket)
		{
			return;
		}
#ifdef _WIN32
		::closesocket(handle);
#else
		::close(handle);
#endif
	}

	int get_last_error()
	{
#ifdef _WIN32
		return WSAGetLastError();
#else
		return errno;
#endif
	}

	bool would_block(int error_code)
	{
#ifdef _WIN32
		return error_code == WSAEWOULDBLOCK;
#else
		return error_code == EAGAIN || error_code == EWOULDBLOCK;
#endif
	}
}
