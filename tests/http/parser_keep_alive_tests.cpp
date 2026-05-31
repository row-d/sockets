#include <string>

#include "lib/net/http/parser.hpp"
#include "tests/support/test_utils.hpp"

namespace tests
{
    void run_parser_keep_alive_tests()
    {
        using net::http::ParseStatus;
        using net::http::Request;

        {
            std::string raw = "GET / HTTP/1.1\r\nConnection: close\r\n\r\n";
            Request request;
            std::size_t consumed = 0;
            ParseStatus status = net::http::parse_request(raw, request, consumed);
            assert_true(status == ParseStatus::Ok, "keep-alive parse ok");
            assert_true(!net::http::should_keep_alive(request), "connection close detected");
        }

        {
            std::string raw = "get /lower HTTP/1.1\r\n\r\n";
            Request request;
            std::size_t consumed = 0;
            ParseStatus status = net::http::parse_request(raw, request, consumed);
            assert_true(status == ParseStatus::Ok, "method normalization parse ok");
            assert_true(request.method == "GET", "method normalized to upper");
            assert_true(net::http::should_keep_alive(request), "keep-alive default true");
        }
    }
}
