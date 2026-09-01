#pragma once
#include <string>

enum class LaunchState
{
    Idle,
    Launching,
    Running,
    Error
};

class c_launch_game
{
public:
    static void Launch(const std::string& buildPath);
    static void Stop();
};

inline c_launch_game launch_game;
