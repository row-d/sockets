#include <string>

#include "lib/net/http/parser.hpp"
#include "tests/support/test_utils.hpp"

namespace tests
{
    void run_parser_chunked_tests()
    {
        using net::http::ParseStatus;
        using net::http::Request;

        {
            std::string raw = "POST /chunk HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
                              "4\r\nWiki\r\n"
                              "5\r\npedia\r\n"
                              "0\r\n\r\n";
            Request request;
            std::size_t consumed = 0;
            ParseStatus status = net::http::parse_request(raw, request, consumed);
            assert_true(status == ParseStatus::Ok, "chunked parse ok");
            assert_true(request.body == "Wikipedia", "chunked body parsed");
            assert_true(consumed == raw.size(), "chunked consumed length matches");
        }

        {
            std::string raw = "POST /chunk HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
                              "4\r\nWi";
            Request request;
            std::size_t consumed = 0;
            ParseStatus status = net::http::parse_request(raw, request, consumed);
            assert_true(status == ParseStatus::NeedMoreData, "chunked partial needs more");
        }
    }
}
