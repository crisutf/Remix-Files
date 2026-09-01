#pragma once
#include <unordered_map>
#include <cstdint>

class c_anim
{
public:
    static float Get(uint32_t id, float target, float speed = 8.f);
    static void Set(uint32_t id, float value);
};

inline c_anim anim;
