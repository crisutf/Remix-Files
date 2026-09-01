#include <ShlObj.h>
#include <source/includes.hpp>
#include <source/install/install.hpp>
#include <source/ui/assets/assets.hpp>
#include <source/ui/pages/include/downloads.hpp>
#include <source/ui/pages/include/friends.hpp>
#include <source/ui/pages/include/home.hpp>
#include <source/ui/pages/include/library.hpp>
#include <source/ui/pages/include/login.hpp>
#include <source/ui/pages/include/sidebar.hpp>
#include <source/ui/render.hpp>
#include <source/utils/utils.hpp>


LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    switch (msg)
    {
    case WM_NCHITTEST:
    {
        POINT cursor_pos = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        ScreenToClient(hwnd, &cursor_pos);
        if (cursor_pos.y >= 0 && cursor_pos.y < (int)r_titlebar_height && cursor_pos.x < (int)(r_width - sc(92.f)))
            return HTCAPTION;
        return HTCLIENT;
    }
    case WM_DPICHANGED:
    {
        float new_scale = HIWORD(wparam) / 96.f;
        if (new_scale != r_dpi_scale)
        {
            r_dpi_scale = new_scale;
            r_sidebar_width = 220.f * r_dpi_scale;
            r_titlebar_height = 40.f * r_dpi_scale;

            RECT* suggested = (RECT*)lparam;
            int new_w = suggested->right - suggested->left;
            int new_h = suggested->bottom - suggested->top;
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, new_w, new_h, SWP_NOZORDER | SWP_NOACTIVATE);
            r_width = (float)new_w;
            r_height = (float)new_h;
            r_dpi_changed = true;
        }
        return 0;
    }
    case WM_SIZE:
        if (wparam == SIZE_MINIMIZED)
            return 0;
        r_resize_width = LOWORD(lparam);
        r_resize_height = HIWORD(lparam);
        return 0;
    case WM_DISPLAYCHANGE:
    {
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        int new_x = (screen_w - (int)r_width) / 2;
        int new_y = (screen_h - (int)r_height) / 2;
        SetWindowPos(hwnd, nullptr, new_x, new_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void DestroyRenderTarget()
{
    if (r_render_target_view)
    {
        r_render_target_view->Release();
        r_render_target_view = nullptr;
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* back_buffer = nullptr;
    if (SUCCEEDED(r_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))))
    {
        r_device->CreateRenderTargetView(back_buffer, nullptr, &r_render_target_view);
        back_buffer->Release();
    }
}

void DestroyD3DDevice()
{
    DestroyRenderTarget();

    if (r_swap_chain)
    {
        r_swap_chain->Release();
        r_swap_chain = nullptr;
    }

    if (r_device_context)
    {
        r_device_context->Release();
        r_device_context = nullptr;
    }

    if (r_device)
    {
        r_device->Release();
        r_device = nullptr;
    }
}

bool CreateD3DDevice(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL feature_level;
    const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, feature_levels, 2, D3D11_SDK_VERSION, &sd, &r_swap_chain, &r_device, &feature_level, &r_device_context);
    if (result == DXGI_ERROR_UNSUPPORTED)
        result = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, feature_levels, 2, D3D11_SDK_VERSION, &sd, &r_swap_chain, &r_device, &feature_level, &r_device_context);
    if (result != S_OK)
        return false;
    CreateRenderTarget();
    return true;
}

static void DrawTitlebar(ImDrawList* dl)
{
    if (r_auth_done)
    {
        static const char* page_names[] = { "Home", "Library", "Settings", "Friends", "Downloads" };
        const char* page_name = (r_page >= 0 && r_page < 5) ? page_names[r_page] : "";

        float t = anim.Get(0xF100 + r_page, 1.f, 10.f);
        int a = (int)(t * 160.f);

        ImFont* tf = r_font_main ? r_font_main : ImGui::GetFont();
        float fsz = sc(13.f);
        ImVec2 tsz = tf->CalcTextSizeA(fsz, FLT_MAX, 0.f, page_name);
        float tx = r_sidebar_width + (r_width - r_sidebar_width - sc(92.f) - tsz.x) * 0.5f;
        float ty = (r_titlebar_height - tsz.y) * 0.5f;
        dl->AddText(tf, fsz, ImVec2(tx, ty), IM_COL32(180, 178, 200, a), page_name);
    }

    ImVec2 min_pos(r_width - sc(92.f), 0.f);
    ImGui::SetCursorPos(min_pos);
    ImGui::InvisibleButton("##min", ImVec2(sc(46.f), r_titlebar_height));
    bool min_hovered = ImGui::IsItemHovered();
    if (min_hovered)
        dl->AddRectFilled(min_pos, ImVec2(min_pos.x + sc(46.f), r_titlebar_height), IM_COL32(255, 255, 255, 18));
    if (ImGui::IsItemClicked())
        ShowWindow(r_window, SW_MINIMIZE);
    {
        float cx = min_pos.x + sc(23.f), cy = r_titlebar_height * 0.5f;
        ImU32 col = min_hovered ? IM_COL32(255, 255, 255, 220) : IM_COL32(100, 98, 120, 255);
        dl->AddLine(ImVec2(cx - sc(5.f), cy), ImVec2(cx + sc(5.f), cy), col, 1.5f);
    }

    ImVec2 cls_pos(r_width - sc(46.f), 0.f);
    ImGui::SetCursorPos(cls_pos);
    ImGui::InvisibleButton("##cls", ImVec2(sc(46.f), r_titlebar_height));
    bool cls_hovered = ImGui::IsItemHovered();
    if (cls_hovered)
        dl->AddRectFilled(cls_pos, ImVec2(cls_pos.x + sc(46.f), r_titlebar_height), IM_COL32(196, 43, 28, 210));
    if (ImGui::IsItemClicked())
        PostQuitMessage(0);
    {
        float cx = cls_pos.x + sc(23.f), cy = r_titlebar_height * 0.5f;
        ImU32 col = cls_hovered ? IM_COL32(255, 255, 255, 235) : IM_COL32(100, 98, 120, 255);
        dl->AddLine(ImVec2(cx - sc(5.f), cy - sc(5.f)), ImVec2(cx + sc(5.f), cy + sc(5.f)), col, 1.5f);
        dl->AddLine(ImVec2(cx + sc(5.f), cy - sc(5.f)), ImVec2(cx - sc(5.f), cy + sc(5.f)), col, 1.5f);
    }
}

static void Render()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(r_width, r_height));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##main", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoScrollWithMouse);

    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(r_width, r_height), IM_COL32(15, 14, 24, 255));
    dl->AddRect(ImVec2(0.5f, 0.5f), ImVec2(r_width - 0.5f, r_height - 0.5f), IM_COL32(35, 33, 52, 255));

    static int prev_page = -1;
    static bool prev_auth = false;

    if (!r_auth_done)
    {
        if (r_page != prev_page || r_auth_done != prev_auth)
        {
            anim.Set(0xF000, 0.f);

            if (!r_auth_done && r_auth_done != prev_auth)
            {
                anim.Set(0x2000, 0.f);
                anim.Set(0x2001, 0.f);
                anim.Set(0x2002, 0.f);
                anim.Set(0x2003, 0.f);
            }

            prev_page = r_page;
            prev_auth = r_auth_done;
        }

        login.Draw(dl);
    }
    else
    {
        sidebar.Draw(dl);

        if (r_page != prev_page || r_auth_done != prev_auth)
        {
            anim.Set(0xF000, 0.f);

            if (r_page == 0)
            {
                anim.Set(0x5000, 0.f);
                anim.Set(0x3F00, 0.f);
                for (int n = 0; n < 3; n++)
                    anim.Set(0x3000 + n, 0.f);
            }
            if (r_page == 1)
                for (int i = 0; i < (int)r_builds.size(); i++)
                    anim.Set(0x4000 + i, 0.f);
            if (r_page == 3)
                for (int i = 0; i < 16; i++)
                    anim.Set(0x7000 + i, 0.f);
            if (r_page == 4)
                for (int i = 0; i < 16; i++)
                    anim.Set(0x8000 + i, 0.f);

            prev_page = r_page;
            prev_auth = r_auth_done;
        }

        float cx = r_sidebar_width + sc(14.f);
        float cw = r_width - r_sidebar_width - sc(28.f);
        float hy = r_titlebar_height + sc(16.f);

        if (r_page == 0)
            home.Draw(dl, cx, cw);
        else if (r_page == 1)
            library.Draw(dl, cx, cw, hy);
        else if (r_page == 3)
            friends.Draw(dl, cx, cw, hy);
        else if (r_page == 4)
            downloads.Draw(dl, cx, cw, hy);
    }

    if (r_logout_confirm)
    {
        float pop_t = anim.Get(0x1100, 1.f, 14.f);
        int malpha = (int)(pop_t * 255.f);

        dl->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(r_width, r_height), IM_COL32(0, 0, 0, (int)(pop_t * 130.f)));

        float modal_w = sc(300.f);
        float modal_h = sc(158.f);
        float modal_x = (r_width - modal_w) * 0.5f;
        float modal_y = (r_height - modal_h) * 0.5f;
        float scale = 0.88f + pop_t * 0.12f;
        float sw = modal_w * scale;
        float sh = modal_h * scale;
        float sx = modal_x + (modal_w - sw) * 0.5f;
        float sy = modal_y + (modal_h - sh) * 0.5f;

        ImVec2 m_min(sx, sy);
        ImVec2 m_max(sx + sw, sy + sh);
        dl->AddRectFilled(m_min, m_max, IM_COL32(28, 26, 42, malpha), sc(10.f));
        dl->AddRect(m_min, m_max, IM_COL32(55, 50, 80, malpha), sc(10.f));

        const char* mtitle = "Log out?";
        ImVec2 mtitle_sz = ImGui::CalcTextSize(mtitle);
        dl->AddText(ImVec2(sx + (sw - mtitle_sz.x) * 0.5f, sy + sc(22.f)), IM_COL32(220, 215, 240, malpha), mtitle);

        const char* msub = "You'll need to sign in again next time.";
        ImVec2 msub_sz = ImGui::CalcTextSize(msub);
        dl->AddText(ImVec2(sx + (sw - msub_sz.x) * 0.5f, sy + sc(46.f)), IM_COL32(90, 85, 115, malpha), msub);

        float btn_w = sc(116.f);
        float btn_h = sc(34.f);
        float btn_y = sy + sh - btn_h - sc(18.f);

        ImVec2 cancel_min(sx + sc(16.f), btn_y);
        ImVec2 cancel_max(cancel_min.x + btn_w, btn_y + btn_h);
        float ct = anim.Get(0x1101, ImGui::IsMouseHoveringRect(cancel_min, cancel_max) ? 1.f : 0.f, 12.f);
        dl->AddRectFilled(cancel_min, cancel_max, IM_COL32((int)(35 + ct * 15), (int)(33 + ct * 12), (int)(52 + ct * 22), malpha), sc(6.f));
        dl->AddRect(cancel_min, cancel_max, IM_COL32(55, 50, 80, malpha), sc(6.f));
        ImVec2 cancel_sz = ImGui::CalcTextSize("Cancel");
        dl->AddText(ImVec2(cancel_min.x + (btn_w - cancel_sz.x) * 0.5f, cancel_min.y + (btn_h - cancel_sz.y) * 0.5f), IM_COL32(200, 195, 218, malpha), "Cancel");
        ImGui::SetCursorPos(cancel_min);
        if (ImGui::InvisibleButton("##logout_cancel", ImVec2(btn_w, btn_h)))
        {
            r_logout_confirm = false;
            anim.Set(0x1100, 0.f);
            anim.Set(0x1101, 0.f);
            anim.Set(0x1102, 0.f);
        }

        ImVec2 lout_min(sx + sw - sc(16.f) - btn_w, btn_y);
        ImVec2 lout_max(lout_min.x + btn_w, btn_y + btn_h);
        float lct = anim.Get(0x1102, ImGui::IsMouseHoveringRect(lout_min, lout_max) ? 1.f : 0.f, 12.f);
        dl->AddRectFilled(lout_min, lout_max, IM_COL32((int)(160 + lct * 60), (int)(30 - lct * 10), (int)(30 - lct * 10), malpha), sc(6.f));
        ImVec2 lout_sz = ImGui::CalcTextSize("Log out");
        dl->AddText(ImVec2(lout_min.x + (btn_w - lout_sz.x) * 0.5f, lout_min.y + (btn_h - lout_sz.y) * 0.5f), IM_COL32(255, 255, 255, malpha), "Log out");
        ImGui::SetCursorPos(lout_min);
        if (ImGui::InvisibleButton("##logout_confirm", ImVec2(btn_w, btn_h)))
        {
            r_logout_confirm = false;
            anim.Set(0x1100, 0.f);
            anim.Set(0x1101, 0.f);
            anim.Set(0x1102, 0.f);
            api.ClearSession();
            r_auth_done = false;
            r_account_name = "";
            r_account_id = "";
            r_auth_token = "";
            r_profile_picture_url = "";
            if (r_profile_texture)
            {
                r_profile_texture->Release();
                r_profile_texture = nullptr;
            }
            r_friends.clear();
            r_friend_ids.clear();
        }
    }
    else
    {
        anim.Set(0x1100, 0.f);
        anim.Set(0x1101, 0.f);
        anim.Set(0x1102, 0.f);
    }

    float fade = 1.f - anim.Get(0xF000, 1.f, 11.f);

    if (fade > 0.002f)
    {
        ImVec2 fade_min = r_auth_done ? ImVec2(r_sidebar_width, 0.f) : ImVec2(0.f, 0.f);
        dl->AddRectFilled(fade_min, ImVec2(r_width, r_height), IM_COL32(15, 14, 24, (int)(fade * 255.f)));
    }

    toast.Draw(dl);
    DrawTitlebar(dl);

    if (r_downloading)
    {
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(r_width, r_height), IM_COL32(0, 0, 0, 180));

        float card_w = sc(420.f);
        float card_h = sc(160.f);
        float cx = (r_width - card_w) * 0.5f;
        float cy = (r_height - card_h) * 0.5f;
        float rad = sc(10.f);

        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + card_w, cy + card_h), IM_COL32(18, 16, 28, 255), rad);
        dl->AddRect(ImVec2(cx, cy), ImVec2(cx + card_w, cy + card_h), IM_COL32(50, 44, 80, 255), rad);

        ImFont* tf = r_font_main ? r_font_main : ImGui::GetFont();
        float pad = sc(20.f);

        dl->AddText(tf, sc(15.f), ImVec2(cx + pad, cy + pad), IM_COL32(220, 218, 245, 255), "Downloading Required Files");

        char sub[64];
        snprintf(sub, sizeof(sub), "File %d of %d", r_download_file_index, r_download_file_total);
        dl->AddText(tf, sc(12.f), ImVec2(cx + pad, cy + pad + sc(20.f)), IM_COL32(110, 108, 140, 255), sub);

        float name_y = cy + pad + sc(46.f);
        float pct = r_download_progress * 100.f;
        char pct_str[16];
        snprintf(pct_str, sizeof(pct_str), "%.0f%%", pct);

        if (!r_download_file_name.empty())
            dl->AddText(tf, sc(12.f), ImVec2(cx + pad, name_y), IM_COL32(190, 188, 215, 255), r_download_file_name.c_str());

        ImVec2 pct_sz = tf->CalcTextSizeA(sc(12.f), FLT_MAX, 0.f, pct_str);
        dl->AddText(tf, sc(12.f), ImVec2(cx + card_w - pad - pct_sz.x, name_y), IM_COL32(190, 188, 215, 255), pct_str);

        float bar_x = cx + pad;
        float bar_y = name_y + sc(20.f);
        float bar_w = card_w - pad * 2.f;
        float bar_h = sc(6.f);
        float fill_w = bar_w * max(0.f, min(1.f, r_download_progress));

        dl->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_w, bar_y + bar_h), IM_COL32(36, 32, 58, 255), bar_h * 0.5f);
        if (fill_w > 0.f)
        {
            dl->AddRectFilledMultiColor(ImVec2(bar_x, bar_y), ImVec2(bar_x + fill_w, bar_y + bar_h), IM_COL32(100, 40, 220, 255), IM_COL32(150, 80, 255, 255),
                IM_COL32(150, 80, 255, 255), IM_COL32(100, 40, 220, 255));
        }

        float mb_dl = r_download_bytes / (1024.f * 1024.f);
        float mb_tot = r_download_total_bytes / (1024.f * 1024.f);
        char mb_str[64];
        if (r_download_total_bytes > 0)
            snprintf(mb_str, sizeof(mb_str), "%.2f MB / %.2f MB", mb_dl, mb_tot);
        else
            snprintf(mb_str, sizeof(mb_str), "%.2f MB", mb_dl);

        dl->AddText(tf, sc(11.f), ImVec2(cx + pad, bar_y + sc(12.f)), IM_COL32(90, 88, 115, 255), mb_str);
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

bool c_render::init()
{
    (void)CoInitialize(nullptr);
    ImGui_ImplWin32_EnableDpiAwareness();

    r_dpi_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT { 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    r_sidebar_width *= r_dpi_scale;
    r_titlebar_height *= r_dpi_scale;

    int phys_w = (int)(r_launcher_width * r_dpi_scale);
    int phys_h = (int)(r_launcher_height * r_dpi_scale);

    r_width = (float)phys_w;
    r_height = (float)phys_h;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"RemixLauncher";
    wc.hIcon = LoadIconW(wc.hInstance, L"IDI_ICON1");
    wc.hIconSm = LoadIconW(wc.hInstance, L"IDI_ICON1");
    RegisterClassExW(&wc);

    int start_x = (GetSystemMetrics(SM_CXSCREEN) - phys_w) / 2;
    int start_y = (GetSystemMetrics(SM_CYSCREEN) - phys_h) / 2;

    r_window = CreateWindowExW(0, wc.lpszClassName, L"Remix Launcher", WS_POPUP | WS_VISIBLE, start_x, start_y, phys_w, phys_h, nullptr, nullptr, wc.hInstance, nullptr);

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(r_window, &margins);

    DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(r_window, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

    if (!CreateD3DDevice(r_window))
    {
        DestroyD3DDevice();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    ShowWindow(r_window, SW_SHOW);
    UpdateWindow(r_window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowBorderSize = 0.f;
    style.FrameBorderSize = 1.f;
    style.ScrollbarSize = 5.f;
    style.FrameRounding = 6.f;
    style.GrabRounding = 4.f;
    style.PopupRounding = 6.f;
    style.ScaleAllSizes(r_dpi_scale);

    ImFontConfig main_cfg;
    main_cfg.FontDataOwnedByAtlas = false;
    r_font_main = io.Fonts->AddFontFromMemoryTTF(geist, sizeof(geist), 15.f * r_dpi_scale, &main_cfg);

    ImFontConfig icon_cfg;
    icon_cfg.FontDataOwnedByAtlas = false;
    icon_cfg.PixelSnapH = true;
    icon_cfg.OversampleH = 2;
    icon_cfg.OversampleV = 2;
    static const ImWchar icon_ranges[] = { 0xf000, 0xf3ff, 0 };
    r_font_icon = io.Fonts->AddFontFromMemoryTTF(icons, sizeof(icons), 16.f * r_dpi_scale, &icon_cfg, icon_ranges);

    ImGui_ImplWin32_Init(r_window);
    ImGui_ImplDX11_Init(r_device, r_device_context);

    std::wstring assets_dir = utils.GetAssetsDir();
    CreateDirectoryW(assets_dir.c_str(), nullptr);
    utils.EnsureAssetFile(assets_dir + L"logo.png", logo_image, sizeof(logo_image));
    utils.EnsureAssetFile(assets_dir + L"splash.png", splash, sizeof(splash));

    r_logo_texture = utils.LoadImageFromFile((assets_dir + L"logo.png").c_str(), &r_logo_width, &r_logo_height);

    {
        BannerEntry be;
        be.texture = utils.LoadImageFromMemory(banner_image, sizeof(banner_image), &be.width, &be.height);
        if (be.texture)
        {
            r_banners.push_back(be);
            r_banner_texture = be.texture;
            r_banner_width = be.width;
            r_banner_height = be.height;
        }
    }

    utils.LoadBuilds();
    api.RestoreSession();
    installer.CheckForUpdate();

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (r_occluded && r_swap_chain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            Sleep(10);
            continue;
        }

        r_occluded = false;

        if (r_resize_width && r_resize_height)
        {
            DestroyRenderTarget();
            r_swap_chain->ResizeBuffers(0, r_resize_width, r_resize_height, DXGI_FORMAT_UNKNOWN, 0);
            r_resize_width = r_resize_height = 0;
            CreateRenderTarget();
        }

        if (r_dpi_changed)
        {
            r_dpi_changed = false;

            ImGuiIO& io = ImGui::GetIO();
            ImGui_ImplDX11_InvalidateDeviceObjects();
            io.Fonts->Clear();

            ImFontConfig main_cfg;
            main_cfg.FontDataOwnedByAtlas = false;
            r_font_main = io.Fonts->AddFontFromMemoryTTF(geist, sizeof(geist), 15.f * r_dpi_scale, &main_cfg);

            ImFontConfig icon_cfg;
            icon_cfg.FontDataOwnedByAtlas = false;
            icon_cfg.PixelSnapH = true;
            icon_cfg.OversampleH = 2;
            icon_cfg.OversampleV = 2;
            static const ImWchar icon_ranges[] = { 0xf000, 0xf3ff, 0 };
            r_font_icon = io.Fonts->AddFontFromMemoryTTF(icons, sizeof(icons), 16.f * r_dpi_scale, &icon_cfg, icon_ranges);

            ImGui_ImplDX11_CreateDeviceObjects();

            ImGuiStyle& style = ImGui::GetStyle();
            style = ImGuiStyle();
            style.WindowBorderSize = 0.f;
            style.FrameBorderSize = 1.f;
            style.ScrollbarSize = 5.f;
            style.FrameRounding = 6.f;
            style.GrabRounding = 4.f;
            style.PopupRounding = 6.f;
            style.ScaleAllSizes(r_dpi_scale);
        }

        if (r_profile_picture_pending && !r_pending_profile_bytes.empty())
        {
            r_profile_picture_pending = false;
            int w = 0, h = 0;
            auto* srv = utils.LoadImageFromMemory(r_pending_profile_bytes.data(), r_pending_profile_bytes.size(), &w, &h);
            if (srv)
            {
                if (r_profile_texture)
                    r_profile_texture->Release();
                r_profile_texture = srv;
                r_profile_width = w;
                r_profile_height = h;
            }
            r_pending_profile_bytes.clear();
        }

        {
            std::vector<std::pair<std::string, std::vector<unsigned char>>> to_upload;
            {
                std::lock_guard<std::mutex> lk(r_friend_avatars_mtx);
                for (auto& kv : r_friend_avatars)
                {
                    if (kv.second.pending && !kv.second.pendingBytes.empty())
                    {
                        to_upload.emplace_back(kv.first, std::move(kv.second.pendingBytes));
                        kv.second.pending = false;
                    }
                }
            }
            for (auto& [id, bytes] : to_upload)
            {
                int w = 0, h = 0;
                auto* srv = utils.LoadImageFromMemory(bytes.data(), bytes.size(), &w, &h);
                if (srv)
                {
                    std::lock_guard<std::mutex> lk(r_friend_avatars_mtx);
                    auto& av = r_friend_avatars[id];
                    if (av.texture)
                        av.texture->Release();
                    av.texture = srv;
                    av.width = w;
                    av.height = h;
                }
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::PushFont(r_font_main);
        ImGui::PopFont();

        Render();

        ImGui::Render();
        float clear_color[4] = { 0.059f, 0.055f, 0.094f, 1.f };
        r_device_context->OMSetRenderTargets(1, &r_render_target_view, nullptr);
        r_device_context->ClearRenderTargetView(r_render_target_view, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        HRESULT present_result = r_swap_chain->Present(1, 0);
        r_occluded = (present_result == DXGI_STATUS_OCCLUDED);

        bool is_focused = (GetForegroundWindow() == r_window);
        static ULONGLONG last_time = GetTickCount64();
        ULONGLONG now = GetTickCount64();
        ULONGLONG target_ms = is_focused ? 0ULL : 33ULL;

        if (now - last_time < target_ms)
        {
            Sleep((DWORD)(target_ms - (now - last_time)));
            last_time = now + (target_ms - (now - last_time));
        }
        else
        {
            last_time = now;
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyD3DDevice();

    DestroyWindow(r_window);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return true;
}
