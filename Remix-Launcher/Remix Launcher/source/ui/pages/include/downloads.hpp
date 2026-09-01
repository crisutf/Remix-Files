#pragma once
#include <source/vendor/imgui/imgui.h>

class c_downloads
{
public:
    static void Draw(ImDrawList* dl, float cx, float cw, float hy);
};

inline c_downloads downloads;
