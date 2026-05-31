#pragma once

#include <iostream>

namespace tests
{
    inline int &failures()
    {
        static int value = 0;
        return value;
    }

    inline void assert_true(bool condition, const char *message)
    {
        if (!condition)
        {
            ++failures();
            std::cerr << "FAIL: " << message << "\n";
        }
    }
}
