#include "response.hpp"

#include <sstream>

namespace net::http
{
    Response::Response(net::socket::SocketHandle client, bool keep_alive) : client_(client), keep_alive_(keep_alive)
    {
    }

    void Response::send(const std::string &body, int status_code, const std::string &content_type)
    {
        send(body, status_code, content_type, keep_alive_);
    }

    void Response::send(const std::string &body, int status_code, const std::string &content_type, bool keep_alive)
    {
        if (sent_)
        {
            return;
        }

        std::string status_text = "OK";
        if (status_code == 404)
        {
            status_text = "Not Found";
        }
        else if (status_code == 500)
        {
            status_text = "Internal Server Error";
        }

        std::ostringstream response;
        response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
        response << "Content-Type: " << content_type << "\r\n";
        response << "Content-Length: " << body.size() << "\r\n";
        response << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
        response << "\r\n";
        response << body;

        std::string payload = response.str();
        std::string error;
        net::socket::send_data(client_, payload.data(), static_cast<int>(payload.size()), &error);
        sent_ = true;
    }

    bool Response::is_sent() const
    {
        return sent_;
    }
}
