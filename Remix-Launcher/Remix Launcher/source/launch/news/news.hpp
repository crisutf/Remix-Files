#pragma once
#include <source/vendor/imgui/imgui.h>

struct NewsItem
{
    const char* title;
    const char* date;
    const char* body;
};

class c_news
{
public:
    static float Draw(ImDrawList* dl, float cx, float cw, float start_y);
};

inline c_news news;
