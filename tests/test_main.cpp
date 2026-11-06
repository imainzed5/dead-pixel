#include "TestFramework.h"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    const std::string filter = argc > 1 ? argv[1] : "";

    int passed = 0;
    int failed = 0;
    for (const dp_test::TestCase& test : dp_test::registry())
    {
        if (!filter.empty() && std::string(test.name).find(filter) == std::string::npos)
        {
            continue;
        }

        try
        {
            test.fn();
            ++passed;
        }
        catch (const dp_test::Failure& failure)
        {
            ++failed;
            std::cerr << "FAIL " << test.name << "\n  " << failure.message << '\n';
        }
        catch (const std::exception& e)
        {
            ++failed;
            std::cerr << "FAIL " << test.name << "\n  exception: " << e.what() << '\n';
        }
    }

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
