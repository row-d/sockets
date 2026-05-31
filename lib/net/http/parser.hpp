#pragma once

#include <cstddef>
#include <string>

#include "request.hpp"

namespace net::http
{
    enum class ParseStatus
    {
        NeedMoreData,
        Ok,
        Error
    };

    ParseStatus parse_request(const std::string &raw, Request &request, std::size_t &consumed);
    std::string get_header_value(const Request &request, const std::string &name);
    bool should_keep_alive(const Request &request);
}
