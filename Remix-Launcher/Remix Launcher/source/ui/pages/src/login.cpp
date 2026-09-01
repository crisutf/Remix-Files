#include <source/ui/pages/include/login.hpp>
#include <source/includes.hpp>

void c_login::Draw(ImDrawList* dl)
{
    float card_w = sc(360.f), card_h = sc(300.f);
    float card_x = (r_width - card_w) * 0.5f;
    float card_y = (r_height - card_h) * 0.5f;

    float card_t = anim.Get(0x2000, 1.f, 12.f);
    float logo_t = anim.Get(0x2001, 1.f, 11.f);
    float btn_t = anim.Get(0x2002, 1.f, 10.f);
    float hint_t = anim.Get(0x2003, 1.f, 10.f);

    if (card_t < 0.5f)  { anim.Set(0x2001, 0.f); anim.Set(0x2002, 0.f); anim.Set(0x2003, 0.f); }
    if (card_t < 0.75f) { anim.Set(0x2002, 0.f); anim.Set(0x2003, 0.f); }
    if (btn_t  < 0.5f)  { anim.Set(0x2003, 0.f); }

    float card_slide = (1.f - card_t) * sc(20.f);
    float logo_slide = (1.f - logo_t) * sc(12.f);
    float btn_slide = (1.f - btn_t) * sc(10.f);

    int card_alpha = (int)(card_t * 255.f);
    int logo_alpha = (int)(logo_t * 255.f);
    int btn_alpha = (int)(btn_t * 255.f);
    int hint_alpha = (int)(hint_t * 200.f);

    float cy = card_y + card_slide;

    dl->AddRectFilled(ImVec2(card_x, cy), ImVec2(card_x + card_w, cy + card_h),
        IM_COL32(18, 17, 28, card_alpha), sc(14.f));
    dl->AddRect(ImVec2(card_x, cy), ImVec2(card_x + card_w, cy + card_h),
        IM_COL32(35, 33, 52, card_alpha), sc(14.f));

    if (r_logo_texture)
    {
        float logo_h = sc(56.f);
        float logo_w = (r_logo_width > 0) ? logo_h * ((float)r_logo_width / (float)r_logo_height) : logo_h;
        float logo_x = card_x + (card_w - logo_w) * 0.5f;
        float ly = cy + sc(56.f) + logo_slide;
        dl->AddImage((ImTextureID)r_logo_texture,
            ImVec2(logo_x, ly), ImVec2(logo_x + logo_w, ly + logo_h),
            ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, logo_alpha));
    }
    else
    {
        ImFont* title_font = r_font_main ? r_font_main : ImGui::GetFont();
        float title_font_sz = r_font_main ? sc(32.f) : sc(22.f);
        ImVec2 title_sz = title_font->CalcTextSizeA(title_font_sz, FLT_MAX, 0.f, "REMIX");
        dl->AddText(title_font, title_font_sz,
            ImVec2(card_x + (card_w - title_sz.x) * 0.5f, cy + sc(72.f) + logo_slide),
            IM_COL32(157, 113, 240, logo_alpha), "REMIX");
    }

    float btn_w = sc(280.f), btn_h = sc(44.f);
    float btn_x = card_x + (card_w - btn_w) * 0.5f;
    float btn_y = cy + sc(152.f) + btn_slide;

    ImGui::SetCursorPos(ImVec2(btn_x, btn_y));
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(88,  101, 242, btn_alpha));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(104, 118, 255, btn_alpha));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(70,  84,  210, btn_alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, sc(8.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    if (ImGui::Button("Continue with Discord", ImVec2(btn_w, btn_h)))
        api.LoginWithDiscord();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    const char* hint = r_discord_polling ? "Waiting for Discord login..." : "Connect your Discord account to play.";
    ImVec2 hint_sz = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2(card_x + (card_w - hint_sz.x) * 0.5f, btn_y + btn_h + sc(18.f)),
        IM_COL32(90, 85, 110, hint_alpha), hint);
}
