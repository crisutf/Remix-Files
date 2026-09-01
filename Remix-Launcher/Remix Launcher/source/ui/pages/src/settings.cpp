#include <source/ui/pages/include/settings.hpp>
#include <source/includes.hpp>
#include <source/utils/utils.hpp>

void c_settings::Draw(ImDrawList* dl, float cx, float cw, float hy)
{
    dl->AddText(ImVec2(cx, hy + sc(7.f)), IM_COL32(255, 255, 255, 255), "Settings");

    float divider_y = r_titlebar_height + sc(62.f);
    dl->AddLine(ImVec2(r_sidebar_width, divider_y), ImVec2(r_width, divider_y), IM_COL32(35, 33, 52, 255));

    dl->AddText(ImVec2(cx, r_height - sc(40.f)), IM_COL32(50, 48, 68, 255), "Remix Launcher v1.0.0  \xc2\xb7  remixogfn.dev");
}
