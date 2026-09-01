#include <source/ui/pages/include/sidebar.hpp>
#include <source/includes.hpp>
#include <source/utils/utils.hpp>

static bool s_account_popup = false;

static constexpr uint32_t k_account_popup_anim = 0x2100;

static void NavigationItem(ImDrawList* dl, const char* label, int index, const char* icon)
{
    float item_h = sc(42.f);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, ImVec2(r_sidebar_width, item_h));
    bool hovered = ImGui::IsItemHovered();
    bool active = r_page == index;

    if (ImGui::IsItemClicked()) r_page = index;

    float bg_t = anim.Get(0xA000 + index, (hovered || active) ? 1.f : 0.f, 10.f);
    float bar_t = anim.Get(0xB000 + index, active ? 1.f : 0.f, 14.f);
    float text_t = anim.Get(0xC000 + index, active ? 1.f : (hovered ? 0.5f : 0.f), 10.f);

    if (bg_t > 0.001f)
        dl->AddRectFilled(pos, ImVec2(pos.x + r_sidebar_width, pos.y + item_h),
            IM_COL32(255, 255, 255, (int)(bg_t * (active ? 12.f : 6.f))));

    if (bar_t > 0.001f)
    {
        float bar_h = bar_t * item_h;
        float bar_y = pos.y + (item_h - bar_h) * 0.5f;
        dl->AddRectFilled(ImVec2(pos.x, bar_y), ImVec2(pos.x + sc(3.f), bar_y + bar_h), IM_COL32(124, 58, 237, 255));
    }

    ImU32 col = IM_COL32(
        (int)(90 + text_t * 165.f),
        (int)(90 + text_t * 165.f),
        (int)(112 + text_t * 143.f),
        255);

    if (r_font_icon && icon)
    {
        float icon_sz_f = sc(16.f);
        ImVec2 icon_sz = r_font_icon->CalcTextSizeA(icon_sz_f, FLT_MAX, 0.f, icon);
        dl->AddText(r_font_icon, icon_sz_f, ImVec2(pos.x + sc(20.f), pos.y + (item_h - icon_sz.y) * 0.5f), col, icon);
    }

    ImVec2 label_sz = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(pos.x + sc(46.f), pos.y + (item_h - label_sz.y) * 0.5f), col, label);
}

void c_sidebar::Draw(ImDrawList* dl)
{
    const float sidebar_bottom_r = sc(12.f);
    dl->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(r_sidebar_width, r_height), IM_COL32(10, 9, 18, 255),
        sidebar_bottom_r, ImDrawFlags_RoundCornersBottom);
    dl->AddLine(ImVec2(r_sidebar_width, 0.f), ImVec2(r_sidebar_width, r_height), IM_COL32(35, 33, 52, 255));

    float layout_h = sc(36.f);
    if (r_logo_texture)
    {
        float logo_h = sc(86.f);
        float logo_w = (r_logo_width > 0) ? logo_h * ((float)r_logo_width / (float)r_logo_height) : logo_h;
        float logo_x = (r_sidebar_width - logo_w) * 0.5f;
        float cy = r_titlebar_height + sc(14.f) + layout_h * 0.5f - sc(8.f);
        dl->AddImage((ImTextureID)r_logo_texture, ImVec2(logo_x, cy - logo_h * 0.5f), ImVec2(logo_x + logo_w, cy + logo_h * 0.5f));
    }
    else
    {
        ImFont* logo_font = r_font_main ? r_font_main : ImGui::GetFont();
        float logo_font_sz = r_font_main ? sc(24.f) : sc(18.f);
        ImVec2 logo_sz = logo_font->CalcTextSizeA(logo_font_sz, FLT_MAX, 0.f, "Remix");
        dl->AddText(logo_font, logo_font_sz, ImVec2((r_sidebar_width - logo_sz.x) * 0.5f, r_titlebar_height + sc(20.f)), IM_COL32(157, 113, 240, 255), "Remix");
    }

    float divider_y = r_titlebar_height + sc(14.f) + layout_h + sc(14.f);
    dl->AddLine(ImVec2(sc(16.f), divider_y), ImVec2(r_sidebar_width - sc(16.f), divider_y), IM_COL32(35, 33, 52, 255));

    ImGui::SetCursorPos(ImVec2(0, divider_y + sc(12.f)));
    NavigationItem(dl, "Home",      0, "\xef\x80\x95");
    NavigationItem(dl, "Library",   1, "\xef\x81\xbb");
    NavigationItem(dl, "Downloads", 4, "\xef\x80\x99");
    NavigationItem(dl, "Friends",   3, "\xef\x83\x80");

    if (r_account_name.empty())
    {
        s_account_popup = false;
        anim.Set(k_account_popup_anim, 0.f);
        return;
    }

    const float card_mx = sc(10.f);
    const float card_mb = sc(10.f);
    const float card_h = sc(56.f);
    const float card_round = sc(10.f);
    const float card_y0 = r_height - card_mb - card_h;
    const ImVec2 card_min(card_mx, card_y0);
    const ImVec2 card_max(r_sidebar_width - card_mx, r_height - card_mb);

    const float pop_gap = sc(12.f);
    const float account_divider_y = card_y0 - sc(7.f);
    dl->AddLine(ImVec2(sc(12.f), account_divider_y), ImVec2(r_sidebar_width - sc(12.f), account_divider_y), IM_COL32(35, 33, 52, 255));

    dl->AddRectFilled(card_min, card_max, IM_COL32(22, 21, 32, 255), card_round);

    float av_r = sc(16.f);
    ImVec2 av_c(card_min.x + sc(12.f) + av_r, (card_min.y + card_max.y) * 0.5f);

    bool section_hov = ImGui::IsMouseHoveringRect(card_min, card_max);
    float hov_t = anim.Get(0x2000, section_hov ? 1.f : 0.f, 10.f);
    if (hov_t > 0.001f)
        dl->AddRectFilled(card_min, card_max, IM_COL32(255, 255, 255, (int)(hov_t * 10.f)), card_round);

    if (r_profile_texture)
    {
        dl->AddImageRounded((ImTextureID)r_profile_texture,
            ImVec2(av_c.x - av_r, av_c.y - av_r), ImVec2(av_c.x + av_r, av_c.y + av_r),
            ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), av_r);
    }
    else
    {
        dl->AddCircleFilled(av_c, av_r, IM_COL32(88, 50, 180, 255));
        char initial[2] = { (char)toupper((unsigned char)r_account_name[0]), '\0' };
        ImVec2 init_sz = ImGui::CalcTextSize(initial);
        dl->AddText(ImVec2(av_c.x - init_sz.x * 0.5f, av_c.y - init_sz.y * 0.5f), IM_COL32(255, 255, 255, 255), initial);
    }

    const char* role_str = r_account_role.empty() ? "Remix Player" : r_account_role.c_str();
    ImVec2 name_sz = ImGui::CalcTextSize(r_account_name.c_str());
    ImVec2 role_sz = ImGui::CalcTextSize(role_str);
    float text_h   = name_sz.y + sc(3.f) + role_sz.y;
    float text_top = av_c.y - text_h * 0.5f;
    float name_x   = av_c.x + av_r + sc(10.f);

    dl->AddText(ImVec2(name_x, text_top), IM_COL32(240, 240, 250, 255), r_account_name.c_str());
    dl->AddText(ImVec2(name_x, text_top + name_sz.y + sc(3.f)), r_account_role_color, role_str);

    if (r_font_icon)
    {
        const char* chev = "\xef\x81\xb8";
        float chev_sz = sc(10.f);
        ImVec2 chev_dim = r_font_icon->CalcTextSizeA(chev_sz, FLT_MAX, 0.f, chev);
        dl->AddText(r_font_icon, chev_sz,
            ImVec2(card_max.x - chev_dim.x - sc(12.f), av_c.y - chev_dim.y * 0.5f),
            IM_COL32(70, 68, 90, 255), chev);
    }

    ImGui::SetCursorScreenPos(card_min);
    ImGui::InvisibleButton("##account_card", ImVec2(card_max.x - card_min.x, card_max.y - card_min.y));
    if (ImGui::IsItemClicked())
        s_account_popup = !s_account_popup;

    const float pop_anim_speed = 8.5f;

    float pop_t = anim.Get(k_account_popup_anim, s_account_popup ? 1.f : 0.f, pop_anim_speed);
    if (pop_t > 0.001f)
    {
        float pop_h = sc(36.f);
        const float pop_round = sc(10.f);
        const float final_pop_y = card_y0 - pop_h - pop_gap;
        const float slide_dist = pop_h + pop_gap + sc(6.f);
        const float u = pop_t * pop_t * (3.f - 2.f * pop_t);
        const float pop_y = final_pop_y + slide_dist * (1.f - u);
        ImVec2 pop_min(card_min.x, pop_y);
        ImVec2 pop_max(card_max.x, pop_y + pop_h);

        const int pop_a = (int)(pop_t * 255.f);
        dl->AddRectFilled(pop_min, pop_max, IM_COL32(22, 21, 32, pop_a), pop_round);
        dl->AddRect(pop_min, pop_max, IM_COL32(40, 38, 58, pop_a), pop_round);

        bool lo_hov = (pop_t >= 0.99f) && ImGui::IsMouseHoveringRect(pop_min, pop_max);
        float lt = anim.Get(0x1000, lo_hov ? 1.f : 0.f, 12.f);
        if (lt > 0.001f)
            dl->AddRectFilled(pop_min, pop_max, IM_COL32(220, 50, 50, (int)(lt * 20.f * pop_t)), pop_round);

        const char* lo_icon = "\xef\x82\x8b";
        const char* lo_text = "  Log out";
        float icon_x = pop_min.x + sc(14.f);
        float text_cy = pop_y + (pop_h - ImGui::CalcTextSize(lo_text).y) * 0.5f;

        ImU32 lo_col = IM_COL32((int)(190 + lt * 65), (int)(188 + lt * 12), (int)(205 + lt * 8), pop_a);

        if (r_font_icon)
        {
            float icon_sz = sc(13.f);
            ImVec2 icon_dim = r_font_icon->CalcTextSizeA(icon_sz, FLT_MAX, 0.f, lo_icon);
            dl->AddText(r_font_icon, icon_sz, ImVec2(icon_x, pop_y + (pop_h - icon_dim.y) * 0.5f), lo_col, lo_icon);
            icon_x += icon_dim.x;
        }

        dl->AddText(ImVec2(icon_x, text_cy), lo_col, lo_text);

        ImGui::SetCursorScreenPos(pop_min);
        ImGui::InvisibleButton("##logout_item", ImVec2(pop_max.x - pop_min.x, pop_h));
        if ((pop_t >= 0.99f) && ImGui::IsItemClicked()) { s_account_popup = false; r_logout_confirm = true; }

        if (ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(pop_min, pop_max) && !section_hov)
            s_account_popup = false;
    }
}
