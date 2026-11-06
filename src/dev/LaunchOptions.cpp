#include "dev/LaunchOptions.h"

#include <charconv>
#include <cstring>
#include <string_view>

namespace
{
template <typename T>
bool parseNumber(std::string_view text, T& out)
{
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc{} && result.ptr == end;
}
} // namespace

bool LaunchOptions::parse(int argc, char* argv[], LaunchOptions& out, std::string& error)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        auto nextValue = [&](std::string_view& value) -> bool
        {
            if (i + 1 >= argc)
            {
                error = std::string(arg) + " needs a value";
                return false;
            }
            value = argv[++i];
            return true;
        };

        std::string_view value;
        if (arg == "--script")
        {
            if (!nextValue(value)) return false;
            out.scriptPath = std::string(value);
        }
        else if (arg == "--shots")
        {
            if (!nextValue(value)) return false;
            out.shotDir = std::string(value);
        }
        else if (arg == "--metrics")
        {
            if (!nextValue(value)) return false;
            out.metricsPath = std::string(value);
        }
        else if (arg == "--world-seed")
        {
            if (!nextValue(value)) return false;
            std::uint32_t seed = 0;
            if (!parseNumber(value, seed))
            {
                error = "--world-seed expects an unsigned integer";
                return false;
            }
            out.worldSeed = seed;
        }
        else if (arg == "--max-ticks")
        {
            if (!nextValue(value)) return false;
            if (!parseNumber(value, out.maxTicks) || out.maxTicks < 0)
            {
                error = "--max-ticks expects a non-negative integer";
                return false;
            }
        }
        else if (arg == "--hidden")
        {
            out.hidden = true;
        }
        else if (arg == "--watch")
        {
            out.watch = true;
        }
        else
        {
            error = "unknown argument: " + std::string(arg);
            return false;
        }
    }

    return true;
}
