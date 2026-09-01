#include <source/includes.hpp>
#include <source/launch/launch_game.hpp>
#include <source/ui/pages/include/library.hpp>
#include <source/utils/utils.hpp>

static int s_confirm_idx = -1;
static int s_active_idx = -1;

void c_library::Draw(ImDrawList* dl, float cx, float cw, float hy)
{
    dl->AddText(ImVec2(cx, hy + sc(7.f)), IM_COL32(255, 255, 255, 255), "Library");

    ImGui::SetCursorPos(ImVec2(r_width - sc(154.f), hy));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(22, 21, 32, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(32, 30, 48, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(124, 58, 237, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, sc(6.f));
    if (ImGui::Button("Import Build", ImVec2(sc(140.f), sc(32.f))))
        utils.AddBuild(utils.BrowseFolder());
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    float divider_y = r_titlebar_height + sc(62.f);
    dl->AddLine(ImVec2(r_sidebar_width, divider_y), ImVec2(r_width, divider_y), IM_COL32(35, 33, 52, 255));

    if (r_builds.empty())
    {
        const char* msg = "No builds imported yet.";
        ImVec2 msg_sz = ImGui::CalcTextSize(msg);
        float empty_y = r_height * 0.45f;

        dl->AddText(ImVec2(r_sidebar_width + (r_width - r_sidebar_width - msg_sz.x) * 0.5f, empty_y), IM_COL32(90, 90, 112, 255), msg);

        float import_w = sc(160.f);
        ImGui::SetCursorPos(ImVec2(r_sidebar_width + (r_width - r_sidebar_width - import_w) * 0.5f, empty_y + sc(32.f)));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(124, 58, 237, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(157, 113, 240, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(100, 30, 200, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, sc(6.f));

        if (ImGui::Button("+ Import Build##e", ImVec2(import_w, sc(36.f))))
            utils.AddBuild(utils.BrowseFolder());

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        return;
    }

    const float card_w = sc(195.f);
    const float img_h = sc(215.f);
    const float info_h = sc(80.f);
    const float card_h = img_h + info_h;
    const float gap = sc(18.f);
    const float pad_x = sc(6.f);
    const float start_y = divider_y + sc(20.f);

    float avail_w = cw - pad_x * 2.f;
    int cols = (int)((avail_w + gap) / (card_w + gap));
    if (cols < 1)
        cols = 1;

    for (int i = 0; i < (int)r_builds.size(); i++)
    {
        int col = i % cols;
        int row = i / cols;
        float card_x = cx + pad_x + col * (card_w + gap);
        float card_y = start_y + row * (card_h + gap);

        float enter_t = anim.Get(0x4000 + i, 1.f, 10.f + i * 1.5f);
        card_y += (1.f - enter_t) * sc(22.f);

        ImVec2 card_min(card_x, card_y);
        ImVec2 card_max(card_x + card_w, card_y + card_h);
        ImVec2 img_max(card_x + card_w, card_y + img_h);
        ImVec2 info_min(card_x, card_y + img_h);

        bool img_hovered = ImGui::IsMouseHoveringRect(card_min, img_max);
        float hover_t = anim.Get(0xD000 + i, img_hovered ? 1.f : 0.f, 10.f);
        int ialpha = (int)(enter_t * 255.f);

        dl->AddRectFilled(card_min, card_max, IM_COL32(22, 21, 32, ialpha), sc(10.f));

        if (r_builds[i].splash_texture)
            dl->AddImageRounded((ImTextureID)r_builds[i].splash_texture, card_min, img_max, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), IM_COL32(255, 255, 255, ialpha), sc(10.f),
                ImDrawFlags_RoundCornersTop);
        else
            dl->AddRectFilled(card_min, img_max, IM_COL32(30, 28, 45, ialpha), sc(10.f), ImDrawFlags_RoundCornersTop);

        if (hover_t > 0.01f)
            dl->AddRectFilled(card_min, img_max, IM_COL32(0, 0, 0, (int)(hover_t * 110.f)), sc(10.f), ImDrawFlags_RoundCornersTop);

        dl->AddRect(card_min, card_max, IM_COL32((int)(35 + hover_t * 55), (int)(33 + hover_t * 20), (int)(52 + hover_t * 80), ialpha), sc(10.f));

        ImGui::SetCursorPos(ImVec2(card_x, card_y));
        char img_id[24];
        sprintf_s(img_id, "##cimg%d", i);
        ImGui::InvisibleButton(img_id, ImVec2(card_w, img_h));
        if (ImGui::IsItemClicked())
        {
            if (s_active_idx == i && r_launch_state != LaunchState::Idle && r_launch_state != LaunchState::Error)
                launch_game.Stop();
            else if (r_launch_state == LaunchState::Idle || r_launch_state == LaunchState::Error)
            {
                s_active_idx = i;
                launch_game.Launch(r_builds[i].path);
            }
        }

        dl->AddRectFilled(info_min, card_max, IM_COL32(26, 24, 40, ialpha), sc(10.f), ImDrawFlags_RoundCornersBottom);
        dl->AddLine(info_min, ImVec2(card_max.x, info_min.y), IM_COL32(35, 33, 52, ialpha));

        std::string ver = r_builds[i].version.empty() ? r_builds[i].name : r_builds[i].version;
        const char* prefix = "++Fortnite+Release-";
        if (ver.rfind(prefix, 0) == 0)
            ver = ver.substr(strlen(prefix));

        std::string ver_top = ver, ver_cl;
        size_t cl_pos = ver.find("-CL-");
        if (cl_pos != std::string::npos)
        {
            ver_top = ver.substr(0, cl_pos);
            ver_cl = "CL-" + ver.substr(cl_pos + 4);
        }

        dl->AddText(ImVec2(card_x + sc(10.f), info_min.y + sc(8.f)), IM_COL32(220, 220, 232, ialpha), ver_top.c_str());
        if (!ver_cl.empty())
            dl->AddText(ImVec2(card_x + sc(10.f), info_min.y + sc(26.f)), IM_COL32(90, 90, 112, ialpha), ver_cl.c_str());

        ImVec2 launch_text_pos(card_x + sc(10.f), info_min.y + sc(50.f));

        bool is_active = (s_active_idx == i);
        bool is_busy = (r_launch_state != LaunchState::Idle && r_launch_state != LaunchState::Error);

        const char* action_label = "Launch";
        if (is_active && r_launch_state == LaunchState::Launching)
            action_label = "Launching...";
        else if (is_active && r_launch_state == LaunchState::Running)
            action_label = "Stop";
        else if (is_active && r_launch_state == LaunchState::Error)
            action_label = "Error";

        bool launch_clickable = !is_busy || (is_active && r_launch_state == LaunchState::Running);
        bool launch_text_hov = launch_clickable && ImGui::IsMouseHoveringRect(launch_text_pos, ImVec2(launch_text_pos.x + sc(80.f), launch_text_pos.y + sc(16.f)));
        float lt = anim.Get(0xE000 + i, launch_text_hov ? 1.f : 0.f, 12.f);

        ImU32 launch_col;
        if (is_active && r_launch_state == LaunchState::Running)
            launch_col = IM_COL32((int)(200 + lt * 55), (int)(50 + lt * -20), (int)(50 + lt * -20), ialpha);
        else if (is_active && r_launch_state == LaunchState::Error)
            launch_col = IM_COL32(220, 50, 50, ialpha);
        else if (is_busy && !is_active)
            launch_col = IM_COL32(60, 57, 80, ialpha);
        else
            launch_col = IM_COL32((int)(90 + lt * 130), (int)(85 + lt * 28), (int)(110 + lt * 130), ialpha);

        if (r_font_icon && !is_busy)
        {
            const char* play_icon = "\xef\x81\x8b";
            ImVec2 icon_sz = r_font_icon->CalcTextSizeA(sc(12.f), FLT_MAX, 0.f, play_icon);
            dl->AddText(r_font_icon, sc(12.f), ImVec2(launch_text_pos.x, launch_text_pos.y + sc(1.f)), launch_col, play_icon);
            dl->AddText(ImVec2(launch_text_pos.x + icon_sz.x + sc(4.f), launch_text_pos.y), launch_col, action_label);
        }
        else
        {
            dl->AddText(launch_text_pos, launch_col, action_label);
        }

        if (launch_clickable)
        {
            ImGui::SetCursorPos(launch_text_pos);
            char lt_id[24];
            sprintf_s(lt_id, "##ltxt%d", i);
            ImGui::InvisibleButton(lt_id, ImVec2(sc(80.f), sc(16.f)));
            if (ImGui::IsItemClicked())
            {
                if (is_active && r_launch_state == LaunchState::Running)
                    launch_game.Stop();
                else if (r_launch_state == LaunchState::Idle || r_launch_state == LaunchState::Error)
                {
                    s_active_idx = i;
                    launch_game.Launch(r_builds[i].path);
                }
            }
        }

        if (is_active && r_launch_state == LaunchState::Idle && s_active_idx == i)
            s_active_idx = -1;

        ImVec2 del_pos(card_x + card_w - sc(28.f), info_min.y + sc(8.f));
        ImGui::SetCursorPos(del_pos);
        char del_id[24];
        sprintf_s(del_id, "##del%d", i);
        ImGui::InvisibleButton(del_id, ImVec2(sc(22.f), sc(22.f)));
        bool del_hov = ImGui::IsItemHovered();
        float del_t = anim.Get(0xF200 + i, del_hov ? 1.f : 0.f, 12.f);
        if (ImGui::IsItemClicked())
            s_confirm_idx = i;

        dl->AddRectFilled(del_pos, ImVec2(del_pos.x + sc(22.f), del_pos.y + sc(22.f)), IM_COL32(220, 38, 38, (int)(del_t * 30.f)), sc(5.f));

        float xpad = sc(6.f);
        ImU32 xcol = IM_COL32((int)(80 + del_t * 175), (int)(75 + del_t * -35), (int)(95 + del_t * -45), ialpha);
        dl->AddLine(ImVec2(del_pos.x + xpad, del_pos.y + xpad), ImVec2(del_pos.x + sc(22.f) - xpad, del_pos.y + sc(22.f) - xpad), xcol, 1.5f);
        dl->AddLine(ImVec2(del_pos.x + sc(22.f) - xpad, del_pos.y + xpad), ImVec2(del_pos.x + xpad, del_pos.y + sc(22.f) - xpad), xcol, 1.5f);
    }

    if (s_confirm_idx >= 0 && s_confirm_idx < (int)r_builds.size())
    {
        float modal_w = sc(300.f), modal_h = sc(140.f);
        float modal_x = r_sidebar_width + (r_width - r_sidebar_width - modal_w) * 0.5f;
        float modal_y = (r_height - modal_h) * 0.5f;

        float pop_t = anim.Get(0xCC01, 1.f, 14.f);

        dl->AddRectFilled(ImVec2(r_sidebar_width, 0.f), ImVec2(r_width, r_height), IM_COL32(0, 0, 0, (int)(pop_t * 120.f)));

        float scale = 0.85f + pop_t * 0.15f;
        float sw = modal_w * scale;
        float sh = modal_h * scale;
        float sx = modal_x + (modal_w - sw) * 0.5f;
        float sy = modal_y + (modal_h - sh) * 0.5f;

        ImVec2 m_min(sx, sy);
        ImVec2 m_max(sx + sw, sy + sh);
        int malpha = (int)(pop_t * 255.f);

        dl->AddRectFilled(m_min, m_max, IM_COL32(28, 26, 42, malpha), sc(10.f));
        dl->AddRect(m_min, m_max, IM_COL32(55, 50, 80, malpha), sc(10.f));

        const char* title = "Remove Build?";
        ImVec2 title_sz = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(sx + (sw - title_sz.x) * 0.5f, sy + sc(18.f)), IM_COL32(220, 220, 232, malpha), title);

        std::string ver = r_builds[s_confirm_idx].version.empty() ? r_builds[s_confirm_idx].name : r_builds[s_confirm_idx].version;
        const char* pfx = "++Fortnite+Release-";
        if (ver.rfind(pfx, 0) == 0)
            ver = ver.substr(strlen(pfx));
        size_t cl = ver.find("-CL-");
        if (cl != std::string::npos)
            ver = ver.substr(0, cl);

        ImVec2 sub_sz = ImGui::CalcTextSize(ver.c_str());
        dl->AddText(ImVec2(sx + (sw - sub_sz.x) * 0.5f, sy + sc(40.f)), IM_COL32(90, 90, 112, malpha), ver.c_str());

        float btn_w = sc(110.f), btn_h = sc(32.f);
        float btn_y = sy + sh - btn_h - sc(16.f);

        ImVec2 cancel_min(sx + sc(16.f), btn_y);
        ImVec2 cancel_max(sx + sc(16.f) + btn_w, btn_y + btn_h);
        bool cancel_hov = ImGui::IsMouseHoveringRect(cancel_min, cancel_max);
        float cancel_t = anim.Get(0xCC02, cancel_hov ? 1.f : 0.f, 12.f);
        dl->AddRectFilled(cancel_min, cancel_max, IM_COL32((int)(35 + cancel_t * 15), (int)(33 + cancel_t * 12), (int)(52 + cancel_t * 20), malpha), sc(6.f));
        dl->AddRect(cancel_min, cancel_max, IM_COL32(55, 50, 80, malpha), sc(6.f));
        ImVec2 cancel_sz = ImGui::CalcTextSize("Cancel");
        dl->AddText(ImVec2(cancel_min.x + (btn_w - cancel_sz.x) * 0.5f, cancel_min.y + (btn_h - cancel_sz.y) * 0.5f), IM_COL32(200, 195, 215, malpha), "Cancel");

        ImGui::SetCursorPos(cancel_min);
        if (ImGui::InvisibleButton("##cancel_confirm", ImVec2(btn_w, btn_h)))
        {
            anim.Set(0xCC01, 0.f);
            s_confirm_idx = -1;
        }

        ImVec2 remove_min(sx + sw - sc(16.f) - btn_w, btn_y);
        ImVec2 remove_max(sx + sw - sc(16.f), btn_y + btn_h);
        bool remove_hov = ImGui::IsMouseHoveringRect(remove_min, remove_max);
        float remove_t = anim.Get(0xCC03, remove_hov ? 1.f : 0.f, 12.f);
        dl->AddRectFilled(remove_min, remove_max, IM_COL32((int)(160 + remove_t * 60), (int)(30 - remove_t * 10), (int)(30 - remove_t * 10), malpha), sc(6.f));
        ImVec2 remove_sz = ImGui::CalcTextSize("Remove");
        dl->AddText(ImVec2(remove_min.x + (btn_w - remove_sz.x) * 0.5f, remove_min.y + (btn_h - remove_sz.y) * 0.5f), IM_COL32(255, 255, 255, malpha), "Remove");

        ImGui::SetCursorPos(remove_min);
        if (ImGui::InvisibleButton("##do_confirm", ImVec2(btn_w, btn_h)))
        {
            if (r_builds[s_confirm_idx].splash_texture)
                r_builds[s_confirm_idx].splash_texture->Release();
            r_builds.erase(r_builds.begin() + s_confirm_idx);
            utils.SaveBuilds();
            toast.Push("Build removed.", TOAST_ERROR);
            anim.Set(0xCC01, 0.f);
            s_confirm_idx = -1;
        }
    }
    else
    {
        anim.Set(0xCC01, 0.f);
        anim.Set(0xCC02, 0.f);
        anim.Set(0xCC03, 0.f);
    }
}
