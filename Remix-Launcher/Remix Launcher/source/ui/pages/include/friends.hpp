#pragma once
#include <source/vendor/imgui/imgui.h>

class c_friends
{
public:
    static void Draw(ImDrawList* dl, float cx, float cw, float hy);
};

inline c_friends friends;
