#include <source/ui/components/include/toast.hpp>
#include <source/includes.hpp>
#include <vector>

struct ToastItem
{
    char text[128];
    ToastType type;
    float elapsed;
    float lifetime;
};

static std::vector<ToastItem> s_toasts;

void c_toast::Push(const char* text, ToastType type)
{
    ToastItem t{};
    strncpy_s(t.text, text, sizeof(t.text) - 1);
    t.type = type;
    t.elapsed = 0.f;
    t.lifetime = 3.f;
    s_toasts.push_back(t);
}

void c_toast::Draw(ImDrawList* dl)
{
    if (s_toasts.empty()) return;

    float dt = ImGui::GetIO().DeltaTime;
    float toast_w = sc(260.f);
    float toast_h = sc(48.f);
    float margin = sc(20.f);
    float gap = sc(8.f);

    for (int i = (int)s_toasts.size() - 1; i >= 0; i--)
        s_toasts[i].elapsed += dt;

    s_toasts.erase(
        std::remove_if(s_toasts.begin(), s_toasts.end(),
            [](const ToastItem& t) { return t.elapsed >= t.lifetime; }),
        s_toasts.end());

    for (int i = 0; i < (int)s_toasts.size(); i++)
    {
        ToastItem& t = s_toasts[i];

        float slide_t = ImMin(t.elapsed / 0.2f, 1.f);
        slide_t = 1.f - (1.f - slide_t) * (1.f - slide_t);
        float fade_t = (t.lifetime - t.elapsed < 0.35f) ? (t.lifetime - t.elapsed) / 0.35f : 1.f;
        float alpha = slide_t * fade_t;
        float x_off = (1.f - slide_t) * sc(32.f);

        float tx = r_width - toast_w - margin + x_off;
        float ty = r_height - margin - toast_h - i * (toast_h + gap);

        ImU32 accent = t.type == TOAST_SUCCESS ? IM_COL32(74, 222, 128, (int)(255 * alpha)): t.type == TOAST_ERROR ? IM_COL32(220, 38, 38, (int)(255 * alpha)):                           IM_COL32(124, 58, 237,  (int)(255 * alpha));

        dl->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + toast_w, ty + toast_h), IM_COL32(22, 21, 32, (int)(240 * alpha)), sc(8.f));

        dl->AddRect(ImVec2(tx, ty), ImVec2(tx + toast_w, ty + toast_h), IM_COL32(35, 33, 52, (int)(255 * alpha)), sc(8.f));

        dl->AddRectFilled(ImVec2(tx, ty + sc(10.f)), ImVec2(tx + sc(3.f), ty + toast_h - sc(10.f)), accent, sc(2.f));

        ImVec2 text_sz = ImGui::CalcTextSize(t.text);
        dl->AddText(ImVec2(tx + sc(14.f), ty + (toast_h - text_sz.y) * 0.5f), IM_COL32(220, 215, 235, (int)(255 * alpha)), t.text);
    }
}
