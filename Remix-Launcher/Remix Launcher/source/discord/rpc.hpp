#pragma once
#include <string>

class c_discord_rpc
{
public:
    void Init(const char* appId);
    void Shutdown();
    void Update();
};

inline c_discord_rpc discord_rpc;
