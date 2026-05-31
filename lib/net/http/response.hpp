#pragma once

#include <string>

#include "lib/net/socket/socket.hpp"

namespace net::http
{
    class Response
    {
    public:
        explicit Response(net::socket::SocketHandle client, bool keep_alive = false);

        void send(const std::string &body, int status_code = 200, const std::string &content_type = "text/plain");
        void send(const std::string &body, int status_code, const std::string &content_type, bool keep_alive);
        bool is_sent() const;

    private:
        net::socket::SocketHandle client_;
        bool keep_alive_{false};
        bool sent_{false};
    };
}