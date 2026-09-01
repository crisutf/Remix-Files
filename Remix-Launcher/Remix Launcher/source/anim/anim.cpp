#include <source/anim/anim.hpp>
#include <source/includes.hpp>

static std::unordered_map<uint32_t, float> s_vals;

float c_anim::Get(uint32_t id, float target, float speed)
{
    float dt = ImGui::GetIO().DeltaTime;
    float& val = s_vals[id];
    val += (target - val) * ImMin(dt * speed, 1.f);
    return val;
}

void c_anim::Set(uint32_t id, float value)
{
    s_vals[id] = value;
}
