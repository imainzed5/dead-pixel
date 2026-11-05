#include "core/Game.h"
#include "dev/LaunchOptions.h"

#include <iostream>

int main(int argc, char* argv[])
{
    LaunchOptions options;
    std::string error;
    if (!LaunchOptions::parse(argc, argv, options, error))
    {
        std::cerr << "dead_pixel_survival: " << error << '\n';
        return 1;
    }

    Game game(std::move(options));
    return game.run();
}
