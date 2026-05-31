#include <cstdlib>
#include <iostream>

#include "tests/support/test_utils.hpp"

namespace tests
{
    void run_parser_content_length_tests();
    void run_parser_keep_alive_tests();
    void run_parser_chunked_tests();
}

int main()
{
    tests::run_parser_content_length_tests();
    tests::run_parser_keep_alive_tests();
    tests::run_parser_chunked_tests();

    if (tests::failures() != 0)
    {
        std::cerr << tests::failures() << " test(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
