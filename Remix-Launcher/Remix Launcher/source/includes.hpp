#pragma once

#include <source/utils/utils.hpp>
#include <source/launch/launch.hpp>
#include <source/api/api.hpp>
#include <source/arc/arc.hpp>
#include <source/vendor/imgui/imgui.h>
#include <source/vendor/imgui/backends/imgui_impl_dx11.h>
#include <source/vendor/imgui/backends/imgui_impl_win32.h>
#include <source/vendor/imgui/imgui_internal.h>

#include <dxgi.h>
#include <vector>
#include <d3d11.h>
#include <dwmapi.h>
#include <string>
#include <windowsx.h>
#include <ShObjIdl_core.h>

#include <source/ui/defines.hpp>

#include <wincodec.h>

#include <source/anim/anim.hpp>
#include <source/ui/components/include/toast.hpp>
#include <source/launch/news/news.hpp>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "windowscodecs.lib")