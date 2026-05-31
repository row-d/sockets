#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "request.hpp"
#include "response.hpp"

namespace net::http
{
    using Handler = std::function<void(Request &, Response &)>;
    class HTTPServer
    {
    public:
        void get(const std::string &path, Handler handler);
        void post(const std::string &path, Handler handler);
        void put(const std::string &path, Handler handler);
        void del(const std::string &path, Handler handler);
        void patch(const std::string &path, Handler handler);
        void head(const std::string &path, Handler handler);
        void options(const std::string &path, Handler handler);
        void set_idle_timeout_ms(int timeout_ms);
        void set_route_idle_timeout_ms(const std::string &path, int timeout_ms);
        void listen(int port);

    private:
        void register_handler(const std::string &method, const std::string &path, Handler handler);

        std::unordered_map<std::string, std::unordered_map<std::string, Handler>> handlers_;
        int idle_timeout_ms_{30000};
        std::unordered_map<std::string, int> route_idle_timeouts_;
    };
}