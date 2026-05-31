#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <sstream>
#include <string>

namespace net::http
{
    namespace
    {
        std::string to_upper(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                           { return static_cast<char>(std::toupper(c)); });
            return value;
        }

        bool parse_request_line(const std::string &line, Request &request)
        {
            std::istringstream stream(line);
            if (!(stream >> request.method >> request.path))
            {
                return false;
            }
            request.method = to_upper(request.method);
            return true;
        }

        std::string to_lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool parse_chunked_body(const std::string &raw, std::size_t start, std::string &body, std::size_t &consumed)
        {
            std::size_t pos = start;
            body.clear();

            while (true)
            {
                auto line_end = raw.find("\r\n", pos);
                if (line_end == std::string::npos)
                {
                    return false;
                }

                std::string size_line = raw.substr(pos, line_end - pos);
                std::size_t semi = size_line.find(';');
                if (semi != std::string::npos)
                {
                    size_line = size_line.substr(0, semi);
                }

                std::size_t chunk_size = 0;
                try
                {
                    chunk_size = static_cast<std::size_t>(std::stoul(size_line, nullptr, 16));
                }
                catch (const std::exception &)
                {
                    return false;
                }

                pos = line_end + 2;
                if (raw.size() < pos + chunk_size + 2)
                {
                    return false;
                }

                body.append(raw.substr(pos, chunk_size));
                pos += chunk_size;

                if (raw.substr(pos, 2) != "\r\n")
                {
                    return false;
                }
                pos += 2;

                if (chunk_size == 0)
                {
                    consumed = pos;
                    return true;
                }
            }
        }
    }

    std::string get_header_value(const Request &request, const std::string &name)
    {
        std::string target = to_lower(name);
        for (const auto &entry : request.headers)
        {
            if (to_lower(entry.first) == target)
            {
                return entry.second;
            }
        }
        return {};
    }

    bool should_keep_alive(const Request &request)
    {
        std::string connection = get_header_value(request, "connection");
        if (connection.empty())
        {
            return true;
        }

        return to_lower(connection) != "close";
    }

    ParseStatus parse_request(const std::string &raw, Request &request, std::size_t &consumed)
    {
        auto header_end = raw.find("\r\n\r\n");
        if (header_end == std::string::npos)
        {
            return ParseStatus::NeedMoreData;
        }
        consumed = header_end + 4;

        std::istringstream stream(raw.substr(0, header_end));
        std::string line;
        if (!std::getline(stream, line))
        {
            return ParseStatus::Error;
        }
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (!parse_request_line(line, request))
        {
            return ParseStatus::Error;
        }

        request.headers.clear();
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            auto colon = line.find(':');
            if (colon == std::string::npos)
            {
                continue;
            }
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            value.erase(0, value.find_first_not_of(' '));
            request.headers.emplace(std::move(key), std::move(value));
        }

        std::string transfer_encoding = get_header_value(request, "transfer-encoding");
        if (!transfer_encoding.empty() && to_lower(transfer_encoding) == "chunked")
        {
            std::size_t chunked_consumed = 0;
            if (!parse_chunked_body(raw, header_end + 4, request.body, chunked_consumed))
            {
                return ParseStatus::NeedMoreData;
            }
            consumed = chunked_consumed;
            return ParseStatus::Ok;
        }

        std::size_t content_length = 0;
        std::string content_length_value = get_header_value(request, "content-length");
        if (!content_length_value.empty())
        {
            try
            {
                content_length = static_cast<std::size_t>(std::stoul(content_length_value));
            }
            catch (const std::exception &)
            {
                return ParseStatus::Error;
            }
        }

        if (raw.size() < header_end + 4 + content_length)
        {
            return ParseStatus::NeedMoreData;
        }

        if (content_length > 0)
        {
            request.body = raw.substr(header_end + 4, content_length);
            consumed = header_end + 4 + content_length;
        }
        else
        {
            request.body.clear();
        }

        return ParseStatus::Ok;
    }
}
