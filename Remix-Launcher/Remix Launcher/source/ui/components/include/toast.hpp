#pragma once
#include <source/vendor/imgui/imgui.h>

enum ToastType
{
    TOAST_INFO = 0,
    TOAST_SUCCESS = 1,
    TOAST_ERROR = 2,
};

class c_toast
{
public:
    static void Push(const char* text, ToastType type = TOAST_INFO);
    static void Draw(ImDrawList* dl);
};

inline c_toast toast;
