#include <source/launch/launch.hpp>
#include <source/utils/utils.hpp>
#include <source/ui/render.hpp>
#include <source/ui/defines.hpp>
#include <source/discord/rpc.hpp>

bool c_launch::init()
{
    if (!utils.GetScreenSize())
        return false;

    discord_rpc.Init("1488731798140883165");

    if (!render.init())
    {
        discord_rpc.Shutdown();
        return false;
    }

    discord_rpc.Shutdown();
    return true;
}
