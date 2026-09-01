#include <source/includes.hpp>
#include <source/launch/news/news.hpp>


static NewsItem news_items[] = {
    { "Remix is live", "Apr 17, 2026", "Hop in, find a squad, and drop into the original map the way you remember it." },
};

float c_news::Draw(ImDrawList* dl, float cx, float cw, float start_y)
{
    ImFont* tf = r_font_main ? r_font_main : ImGui::GetFont();

    int count = (int)(sizeof(news_items) / sizeof(news_items[0]));

    float gap = sc(8.f);
    float card_h = sc(84.f);
    float thumb_w = sc(110.f);
    float radius = sc(6.f);
    float col_w = (cw - gap) * 0.5f;

    float y = start_y;

    for (int n = 0; n < count; n++)
    {
        float et = anim.Get(0x3000 + n, 1.f, 5.f + n * 1.5f);
        int ia = (int)(et * 255.f);
        float y_off = (1.f - et) * sc(8.f);

        int col = n % 2;
        int row = n / 2;
        float cx2 = cx + col * (col_w + gap);
        float cy2 = y + row * (card_h + gap) + y_off;

        ImVec2 cmin(cx2, cy2);
        ImVec2 cmax(cx2 + col_w, cy2 + card_h);

        dl->AddRectFilled(cmin, cmax, IM_COL32(20, 18, 32, ia), radius);
        dl->AddRect(cmin, cmax, IM_COL32(36, 34, 54, ia), radius);

        ImVec2 tmin(cmin.x, cmin.y);
        ImVec2 tmax(cmin.x + thumb_w, cmax.y);

        dl->PushClipRect(tmin, tmax, true);

        ID3D11ShaderResourceView* thumb = nullptr;
        if (!r_banners.empty())
            thumb = r_banners[0].texture;
        else if (r_banner_texture)
            thumb = r_banner_texture;

        if (thumb)
        {
            dl->AddImageRounded((ImTextureID)thumb, tmin, tmax, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, ia), radius);
            dl->AddRectFilled(tmin, tmax, IM_COL32(8, 6, 18, 80), radius);
        }
        else
        {
            dl->AddRectFilledMultiColor(tmin, tmax, IM_COL32(60, 28, 120, ia), IM_COL32(30, 18, 65, ia), IM_COL32(30, 18, 65, ia), IM_COL32(60, 28, 120, ia));
        }

        dl->PopClipRect();

        dl->AddRectFilledMultiColor(ImVec2(tmax.x - sc(28.f), tmin.y), ImVec2(tmax.x + sc(4.f), tmax.y), IM_COL32(20, 18, 32, 0), IM_COL32(20, 18, 32, ia),
            IM_COL32(20, 18, 32, ia), IM_COL32(20, 18, 32, 0));

        float tx = tmax.x + sc(10.f);
        float text_w = cmax.x - tx - sc(8.f);
        float pad_y = sc(10.f);

        float title_sz = sc(15.f);
        float body_sz = sc(12.f);
        float date_sz = sc(11.f);

        dl->AddText(tf, title_sz, ImVec2(tx, cy2 + pad_y), IM_COL32(225, 222, 245, ia), news_items[n].title, nullptr, text_w);

        dl->AddText(tf, body_sz, ImVec2(tx, cy2 + pad_y + title_sz + sc(4.f)), IM_COL32(130, 128, 158, ia), news_items[n].body, nullptr, text_w);

        dl->AddText(tf, date_sz, ImVec2(tx, cmax.y - pad_y - date_sz), IM_COL32(72, 70, 98, ia), news_items[n].date);
    }

    int rows = (count + 1) / 2;
    return y + rows * (card_h + gap);
}
