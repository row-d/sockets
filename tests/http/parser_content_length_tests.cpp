#include <string>

#include "lib/net/http/parser.hpp"
#include "tests/support/test_utils.hpp"

namespace tests
{
    void run_parser_content_length_tests()
    {
        using net::http::ParseStatus;
        using net::http::Request;

        std::string raw = "POST /submit HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
        Request request;
        std::size_t consumed = 0;
        ParseStatus status = net::http::parse_request(raw, request, consumed);
        assert_true(status == ParseStatus::Ok, "content-length parse ok");
        assert_true(request.method == "POST", "method parsed");
        assert_true(request.path == "/submit", "path parsed");
        assert_true(request.body == "hello", "body parsed");
        assert_true(consumed == raw.size(), "consumed length matches");
    }
}
