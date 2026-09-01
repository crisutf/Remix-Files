#include <source/includes.hpp>
#include <source/ui/assets/assets.hpp>
#include <source/ui/pages/include/home.hpp>

static int s_banner_idx = 0;
static float s_banner_switch = 0.f;
static float s_banner_fade = 1.f;

void c_home::Draw(ImDrawList* dl, float cx, float cw)
{
    ImFont* tf = r_font_main ? r_font_main : ImGui::GetFont();
    float t = anim.Get(0x5000, 1.f, 11.f);

    if (!r_account_name.empty())
    {
        float xoff = floor((1.f - t) * -sc(14.f) + 0.5f);
        float av_r = sc(20.f);
        float av_cx = cx + sc(10.f) + av_r + xoff;
        float av_cy = r_titlebar_height + sc(10.f) + av_r;

        if (r_profile_texture)
        {
            dl->AddImageRounded((ImTextureID)r_profile_texture, ImVec2(av_cx - av_r, av_cy - av_r), ImVec2(av_cx + av_r, av_cy + av_r), ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32(255, 255, 255, (int)(t * 255)), av_r);
        }
        else
        {
            dl->AddCircleFilled(ImVec2(av_cx, av_cy), av_r, IM_COL32(88, 50, 180, (int)(t * 255)));
            dl->AddCircle(ImVec2(av_cx, av_cy), av_r, IM_COL32(124, 58, 237, (int)(t * 180)));
            char ini[2] = { (char)toupper((unsigned char)r_account_name[0]), '\0' };
            ImVec2 ini_sz = ImGui::CalcTextSize(ini);
            dl->AddText(ImVec2(av_cx - ini_sz.x * 0.5f, av_cy - ini_sz.y * 0.5f), IM_COL32(255, 255, 255, (int)(t * 255)), ini);
        }

        float tx = av_cx + av_r + sc(10.f);

        SYSTEMTIME st;
        GetLocalTime(&st);
        const char* tod = st.wHour < 12 ? "Good morning, " : st.wHour < 17 ? "Good afternoon, " : "Good evening, ";

        float gsz = sc(18.f);
        ImU32 greet_col = IM_COL32(208, 208, 232, (int)(t * 255));
        ImU32 name_col = IM_COL32(167, 139, 250, (int)(t * 255));

        float text_y = av_cy - gsz * 0.5f;

        dl->AddText(tf, gsz, ImVec2(tx, text_y), greet_col, tod);
        float tod_width = floor(tf->CalcTextSizeA(gsz, FLT_MAX, 0.f, tod).x);
        float name_x = tx + tod_width;

        dl->AddText(tf, gsz, ImVec2(name_x, text_y), name_col, r_account_name.c_str());

        float name_width = floor(tf->CalcTextSizeA(gsz, FLT_MAX, 0.f, r_account_name.c_str()).x);
        float excl_x = name_x + name_width;
        dl->AddText(tf, gsz, ImVec2(excl_x, text_y), greet_col, "!");
    }

    {
        char slabel_buf[32];
        if (r_server_status == 1)
            sprintf_s(slabel_buf, "Offline");
        else if (r_server_status == 2)
            sprintf_s(slabel_buf, "Maintenance");
        else
            sprintf_s(slabel_buf, "%d Online", r_player_count);

        const char* slabel = slabel_buf;
        ImU32 sdot = r_server_status == 1 ? IM_COL32(220, 38, 38, 255) : r_server_status == 2 ? IM_COL32(251, 191, 36, 255) : IM_COL32(74, 222, 128, 255);

        float sp = r_server_status == 0 ? sinf((float)ImGui::GetTime() * 3.f) * 0.5f + 0.5f : 0.f;

        ImVec2 slsz = ImGui::CalcTextSize(slabel);

        float spw = slsz.x + sc(28.f), sph = sc(24.f);
        float spx = cx + cw - spw;
        float spy = r_titlebar_height + sc(18.f);
        float scy = spy + sph * 0.5f;
        dl->AddRectFilled(ImVec2(spx, spy), ImVec2(spx + spw, spy + sph), IM_COL32(22, 21, 32, 255), sc(12.f));
        dl->AddRect(ImVec2(spx, spy), ImVec2(spx + spw, spy + sph), IM_COL32(35, 33, 52, 255), sc(12.f));
        dl->AddCircle(ImVec2(spx + sc(11.f), scy), sc(4.5f) + sp * sc(1.f), ImU32(sdot & 0x00FFFFFF | (ImU32)(30 + sp * 30) << 24));
        dl->AddCircleFilled(ImVec2(spx + sc(11.f), scy), sc(3.5f), sdot);
        dl->AddText(ImVec2(spx + sc(20.f), scy - slsz.y * 0.5f), IM_COL32(198, 198, 218, 255), slabel);
    }

    {
        int banner_count = (int)r_banners.size();
        if (banner_count == 0 && r_banner_texture)
            banner_count = 1;

        if (banner_count >= 2)
        {
            float now = (float)ImGui::GetTime();
            if (s_banner_switch == 0.f)
                s_banner_switch = now;
            if (now - s_banner_switch >= 5.f)
            {
                s_banner_idx = (s_banner_idx + 1) % banner_count;
                s_banner_switch = now;
            }
        }
        else
        {
            s_banner_idx = 0;
        }
    }

    float banner_y = r_titlebar_height + sc(63.f) + (1.f - t) * sc(22.f);
    float banner_h = sc(332.f);
    ImVec2 bmin(cx, banner_y);
    ImVec2 bmax(cx + cw, banner_y + banner_h);

    float clip_y0 = r_titlebar_height + sc(63.f);
    dl->PushClipRect(ImVec2(cx, clip_y0), ImVec2(cx + cw, clip_y0 + banner_h), true);

    dl->AddRectFilled(bmin, bmax, IM_COL32(18, 14, 35, 255), sc(10.f));

    ID3D11ShaderResourceView* cur_tex = nullptr;
    if (!r_banners.empty() && s_banner_idx < (int)r_banners.size())
        cur_tex = r_banners[s_banner_idx].texture;
    else if (r_banner_texture)
        cur_tex = r_banner_texture;

    if (cur_tex)
    {
        int img_w = 1, img_h = 1;
        if (!r_banners.empty() && s_banner_idx < (int)r_banners.size())
        {
            img_w = r_banners[s_banner_idx].width;
            img_h = r_banners[s_banner_idx].height;
        }
        else if (r_banner_width && r_banner_height)
        {
            img_w = r_banner_width;
            img_h = r_banner_height;
        }

        float disp_ar = cw / banner_h;
        float img_ar = (float)img_w / (float)img_h;

        ImVec2 uv0(0.f, 0.f), uv1(1.f, 1.f);
        if (img_ar > disp_ar)
        {
            float visible = disp_ar / img_ar;
            float off = (1.f - visible) * 0.5f;
            uv0.x = off;
            uv1.x = 1.f - off;
        }
        else
        {
            float visible = img_ar / disp_ar;
            float off = (1.f - visible) * 0.5f;
            uv0.y = off;
            uv1.y = 1.f - off;
        }

        dl->AddImageRounded((ImTextureID)cur_tex, bmin, bmax, uv0, uv1, IM_COL32(255, 255, 255, 255), sc(10.f));
    }
    else
    {
        dl->AddRectFilledMultiColor(
            bmin, ImVec2(bmin.x + cw * 0.6f, bmax.y), IM_COL32(60, 30, 120, 80), IM_COL32(18, 14, 35, 0), IM_COL32(18, 14, 35, 0), IM_COL32(60, 30, 120, 80));
    }

    dl->AddRectFilled(bmin, bmax, IM_COL32(8, 6, 18, 55), sc(10.f));

    dl->AddRectFilledMultiColor(ImVec2(bmin.x, bmax.y - sc(210.f)), bmax, IM_COL32(12, 10, 22, 0), IM_COL32(12, 10, 22, 0), IM_COL32(12, 10, 22, 252), IM_COL32(12, 10, 22, 252));

    {
        float r = sc(10.f);
        ImU32 bg = IM_COL32(15, 14, 24, 255);
        dl->PathLineTo(ImVec2(bmin.x, bmax.y));
        dl->PathLineTo(ImVec2(bmin.x + r, bmax.y));
        dl->PathArcTo(ImVec2(bmin.x + r, bmax.y - r), r, IM_PI * 0.5f, IM_PI);
        dl->PathFillConvex(bg);
        dl->PathLineTo(ImVec2(bmax.x - r, bmax.y));
        dl->PathLineTo(ImVec2(bmax.x, bmax.y));
        dl->PathLineTo(ImVec2(bmax.x, bmax.y - r));
        dl->PathArcTo(ImVec2(bmax.x - r, bmax.y - r), r, 0.f, IM_PI * 0.5f);
        dl->PathFillConvex(bg);
    }

    dl->AddRect(bmin, bmax, IM_COL32(35, 33, 52, 160), sc(10.f));

    float ox = cx + sc(26.f);
    float oy = bmax.y - sc(152.f);

    float tsz = sc(32.f);
    dl->AddText(tf, tsz, ImVec2(ox, oy), IM_COL32(255, 255, 255, 255), "Remix");
    oy += tsz + sc(10.f);

    const ImU32 dcol = IM_COL32(165, 165, 195, 255);
    dl->AddText(ImVec2(ox, oy), dcol, "Relive Fortnite Chapter 2 Remix once again in 2026. Wipe out enemies,");
    dl->AddText(ImVec2(ox, oy + sc(18.f)), dcol, "eliminate bosses, and dominate the original Fortnite Chapter 2 map.");
    oy += sc(18.f) * 2.f + sc(18.f);

    ImGui::SetCursorPos(ImVec2(ox, oy));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(124, 58, 237, 30));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(124, 58, 237, 60));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(124, 58, 237, 90));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(124, 58, 237, 200));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, sc(8.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    if (ImGui::Button("Play Now", ImVec2(sc(132.f), sc(38.f))))
        r_page = r_builds.empty() ? 4 : 1;
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    ImGui::SameLine(0.f, sc(10.f));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 255, 255, 14));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 26));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 10));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(208, 208, 232, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, sc(8.f));
    if (ImGui::Button("Discord", ImVec2(sc(100.f), sc(38.f))))
        ShellExecuteA(nullptr, "open", "https://discord.gg/remixfn", nullptr, nullptr, SW_SHOWNORMAL);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    {
        int banner_count = (int)r_banners.size();
        if (banner_count == 0 && r_banner_texture)
            banner_count = 1;

        if (banner_count >= 2)
        {
            float dot_sz = sc(6.f);
            float dot_gap = sc(7.f);
            float active_w = sc(18.f);

            float total = active_w + (banner_count - 1) * dot_sz + (banner_count - 1) * dot_gap;
            float dx = bmax.x - sc(18.f) - total;
            float dy = bmax.y - sc(18.f) - dot_sz * 0.5f;

            for (int d = 0; d < banner_count; d++)
            {
                if (d == s_banner_idx)
                {
                    dl->AddRectFilled(ImVec2(dx, dy - sc(2.f)), ImVec2(dx + active_w, dy + sc(2.f)), IM_COL32(255, 255, 255, 220), sc(2.f));
                    dx += active_w + dot_gap;
                }
                else
                {
                    dl->AddCircleFilled(ImVec2(dx + dot_sz * 0.5f, dy), dot_sz * 0.5f, IM_COL32(255, 255, 255, 55));
                    dx += dot_sz + dot_gap;
                }
            }
        }
    }

    dl->PopClipRect();

    float split_gap = sc(28.f);
    float news_w = (cw - split_gap) * 0.62f;
    float side_x = cx + news_w + split_gap;
    float side_w = cw - news_w - split_gap;
    float below_y = bmax.y + sc(36.f);

    news.Draw(dl, cx, news_w, below_y);

    {
        float ry = below_y;
        float row_h = sc(46.f);
        float av_r = sc(15.f);

        int shown = 0;
        for (int i = 0; i < (int)r_friends.size() && shown < 5; i++)
        {
            auto& fr = r_friends[i];
            if (fr.accountId.empty() || !fr.online)
                continue;

            float et = anim.Get(0x3400 + shown, 1.f, 6.f + shown * 2.f);
            int ia = (int)(et * 255.f);
            float x_off = (1.f - et) * sc(10.f);

            float row_y = ry + shown * row_h + x_off;
            float av_cx = side_x + av_r;
            float av_cy = row_y + av_r + sc(2.f);

            ID3D11ShaderResourceView* tex = nullptr;
            {
                std::lock_guard<std::mutex> lk(r_friend_avatars_mtx);
                auto it = r_friend_avatars.find(fr.accountId);
                if (it != r_friend_avatars.end())
                    tex = it->second.texture;
            }

            if (tex)
            {
                dl->AddImageRounded(
                    (ImTextureID)tex, ImVec2(av_cx - av_r, av_cy - av_r), ImVec2(av_cx + av_r, av_cy + av_r), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, ia), av_r);
            }
            else
            {
                dl->AddCircleFilled(ImVec2(av_cx, av_cy), av_r, IM_COL32(88, 50, 180, ia));
                char ini[2] = { (char)toupper((unsigned char)(fr.displayName.empty() ? '?' : fr.displayName[0])), '\0' };
                ImVec2 isz = ImGui::CalcTextSize(ini);
                dl->AddText(ImVec2(av_cx - isz.x * 0.5f, av_cy - isz.y * 0.5f), IM_COL32(255, 255, 255, ia), ini);
            }

            float dot_cx = av_cx + av_r - sc(2.f);
            float dot_cy = av_cy + av_r - sc(2.f);
            dl->AddCircleFilled(ImVec2(dot_cx, dot_cy), sc(5.f), IM_COL32(15, 14, 24, ia));
            dl->AddCircleFilled(ImVec2(dot_cx, dot_cy), sc(3.2f), IM_COL32(74, 222, 128, ia));

            float text_x = av_cx + av_r + sc(12.f);
            dl->AddText(tf, sc(14.f), ImVec2(text_x, av_cy - sc(10.f)), IM_COL32(230, 228, 245, ia), fr.displayName.c_str());

            const char* activity = (fr.status.empty() || fr.status == "offline") ? "In lobby" : fr.status.c_str();
            dl->AddText(tf, sc(11.f), ImVec2(text_x, av_cy + sc(4.f)), IM_COL32(130, 130, 158, ia), activity);

            shown++;
        }

        if (shown == 0)
        {
            float et = anim.Get(0x3400, 1.f, 8.f);
            int ia = (int)(et * 255.f);
            dl->AddText(tf, sc(13.f), ImVec2(side_x, ry + sc(8.f)), IM_COL32(110, 110, 135, ia), "Nobody's online. Invite a friend.");
        }
    }
}
