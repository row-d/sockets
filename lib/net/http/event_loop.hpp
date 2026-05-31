#pragma once

#include <functional>
#include <string>

#include "lib/net/socket/socket.hpp"

namespace net::http
{
    enum class RequestAction
    {
        NeedMoreData,
        KeepAlive,
        Close
    };

    struct RequestDecision
    {
        RequestAction action{RequestAction::Close};
        int next_idle_timeout_ms{0};
    };

    using RawRequestHandler = std::function<RequestDecision(net::socket::SocketHandle, std::string &)>;

    void run_event_loop(net::socket::SocketHandle listener, int idle_timeout_ms, RawRequestHandler handler);
}
