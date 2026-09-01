#include <source/ui/pages/include/friends.hpp>
#include <source/includes.hpp>

static char s_search[128] = {};
static char s_prev_search[128] = {};

static ImU32 AvatarColor(const char* name)
{
    uint32_t h = 5381;
    for (const char* c = name; *c; c++) h = h * 33 ^ (unsigned char)*c;
    static const ImU32 palette[5] = {
        IM_COL32(88,  50,  180, 255),
        IM_COL32(37,  99,  235, 255),
        IM_COL32(124, 58,  237, 255),
        IM_COL32(5,   150, 105, 255),
        IM_COL32(180, 50,  120, 255),
    };

    return palette[h % 5u];
}

static void DrawFriendCard(ImDrawList* dl, float cx, float cw, float y, const FriendEntry& f, int idx)
{
    const float card_h = sc(56.f);
    const char* name = f.displayName.c_str();

    float enter_t = anim.Get(0x7000 + idx, 1.f, 10.f + idx * 1.5f);
    float x_off = (1.f - enter_t) * sc(24.f);
    ImVec2 cmin(cx + x_off, y);
    ImVec2 cmax(cx + cw + x_off, y + card_h);

    float hover_t = anim.Get(0x7100 + idx, ImGui::IsMouseHoveringRect(cmin, cmax) ? 1.f : 0.f, 10.f);

    dl->AddRectFilled(cmin, cmax,
        IM_COL32((int)(22 + hover_t * 6), (int)(21 + hover_t * 5), (int)(32 + hover_t * 8), 255), sc(8.f));
    dl->AddRect(cmin, cmax,
        IM_COL32((int)(35 + hover_t * 30), (int)(33 + hover_t * 10), (int)(52 + hover_t * 50), 255), sc(8.f));

    float av_r = sc(16.f);
    ImVec2 av_c(cmin.x + sc(18.f) + av_r, y + card_h * 0.5f);

    ID3D11ShaderResourceView* av_tex = nullptr;
    {
        std::lock_guard<std::mutex> lk(r_friend_avatars_mtx);
        auto it = r_friend_avatars.find(f.accountId);
        if (it != r_friend_avatars.end()) av_tex = it->second.texture;
    }

    if (av_tex)
    {
        dl->AddImageRounded((ImTextureID)av_tex,
            ImVec2(av_c.x - av_r, av_c.y - av_r), ImVec2(av_c.x + av_r, av_c.y + av_r),
            ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), av_r);
    }
    else
    {
        dl->AddCircleFilled(av_c, av_r, AvatarColor(name));
        char initial[2] = { (char)toupper((unsigned char)name[0]), '\0' };
        ImVec2 init_sz = ImGui::CalcTextSize(initial);
        dl->AddText(ImVec2(av_c.x - init_sz.x * 0.5f, av_c.y - init_sz.y * 0.5f),
            IM_COL32(255, 255, 255, 255), initial);
    }

    float name_x = av_c.x + av_r + sc(12.f);
    ImVec2 name_sz = ImGui::CalcTextSize(name);
    dl->AddText(ImVec2(name_x, y + (card_h - name_sz.y) * 0.5f),
        IM_COL32(218, 218, 235, 255), name);

    float btn_w = sc(72.f), btn_h = sc(28.f);
    float btn_x = cmax.x - btn_w - sc(12.f);
    float btn_y = y + (card_h - btn_h) * 0.5f;
    ImVec2 bmin(btn_x, btn_y), bmax(btn_x + btn_w, btn_y + btn_h);

    ImGui::SetCursorPos(bmin);
    char bid[32]; sprintf_s(bid, "##rf%d", idx);
    ImGui::InvisibleButton(bid, ImVec2(btn_w, btn_h));
    float bt = anim.Get(0x7300 + idx, ImGui::IsItemHovered() ? 1.f : 0.f, 12.f);

    if (ImGui::IsItemClicked())
    {
        api.RemoveFriend(f.accountId);
        toast.Push("Friend removed.", TOAST_INFO);
    }

    dl->AddRectFilled(bmin, bmax,
        IM_COL32((int)(22 + bt * 10), (int)(21 + bt * 8), (int)(32 + bt * 16), 255), sc(6.f));
    dl->AddRect(bmin, bmax,
        IM_COL32((int)(35 + bt * 105), (int)(33 + bt * 2), (int)(52 + bt * 0), 255), sc(6.f));
    ImVec2 lsz = ImGui::CalcTextSize("Remove");
    dl->AddText(ImVec2(bmin.x + (btn_w - lsz.x) * 0.5f, bmin.y + (btn_h - lsz.y) * 0.5f),
        IM_COL32(198, 198, 218, 255), "Remove");
}

void c_friends::Draw(ImDrawList* dl, float cx, float cw, float hy)
{
    int total = (int)r_friends.size();

    dl->AddText(ImVec2(cx, hy + sc(7.f)), IM_COL32(255, 255, 255, 255), "Friends");
    char cnt_buf[32]; sprintf_s(cnt_buf, "%d friends", total);
    ImVec2 cnt_sz = ImGui::CalcTextSize("Friends");
    dl->AddText(ImVec2(cx + cnt_sz.x + sc(14.f), hy + sc(7.f)), IM_COL32(60, 57, 80, 255), cnt_buf);

    float search_w = cw * 0.3f;
    float search_x = cx + cw - search_w;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(22, 21, 32, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(26, 25, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(26, 25, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_Text,           IM_COL32(218, 218, 235, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   sc(8.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    ImVec2(sc(30.f), sc(7.f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

    ImGui::SetCursorPos(ImVec2(search_x, hy - sc(1.f)));
    ImGui::SetNextItemWidth(search_w);
    ImGui::InputText("##friends_search", s_search, sizeof(s_search));

    bool search_active = ImGui::IsItemActive();
    float focus_t = anim.Get(0x6F00, search_active ? 1.f : 0.f, 10.f);
    ImVec2 ir_min = ImGui::GetItemRectMin();
    ImVec2 ir_max = ImGui::GetItemRectMax();

    dl->AddRect(ir_min, ir_max,
        IM_COL32((int)(35 + focus_t * 89), (int)(33 + focus_t * 25), (int)(52 + focus_t * 185), 255), sc(8.f));

    if (r_font_icon)
    {
        const char* sg = "\xef\x80\x82";
        ImVec2 isz = r_font_icon->CalcTextSizeA(sc(12.f), FLT_MAX, 0.f, sg);
        dl->AddText(r_font_icon, sc(12.f),
            ImVec2(ir_min.x + sc(10.f), ir_min.y + (ir_max.y - ir_min.y - isz.y) * 0.5f),
            IM_COL32((int)(60 + focus_t * 64), 57, (int)(80 + focus_t * 157), 255), sg);
    }

    if (s_search[0] == '\0')
        dl->AddText(ImVec2(ir_min.x + sc(28.f), ir_min.y + (ir_max.y - ir_min.y - ImGui::CalcTextSize("A").y) * 0.5f),
            IM_COL32(55, 52, 72, 255), "Search players...");

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    if (strcmp(s_search, s_prev_search) != 0)
    {
        strcpy_s(s_prev_search, s_search);
        api.SearchPlayers(s_search);
    }

    float divider_y = r_titlebar_height + sc(62.f);
    dl->AddLine(ImVec2(r_sidebar_width, divider_y), ImVec2(r_width, divider_y), IM_COL32(35, 33, 52, 255));

    float list_y = divider_y + sc(14.f);

    if (s_search[0] != '\0')
    {
        dl->AddText(ImVec2(cx, list_y), IM_COL32(90, 90, 112, 255), "RESULTS");
        list_y += ImGui::CalcTextSize("RESULTS").y + sc(8.f);

        if (r_search_results.empty())
        {
            const char* msg = r_searching ? "Searching..." : "No players found.";
            dl->AddText(ImVec2(cx, list_y), IM_COL32(60, 57, 80, 255), msg);
        }
        else
        {
            float btn_w = sc(72.f), btn_h = sc(28.f), btn_gap = sc(6.f);

            for (int i = 0; i < (int)r_search_results.size(); i++)
            {
                const std::string& aid  = r_search_results[i].accountId;
                const char* name = r_search_results[i].name.c_str();
                bool already_friend = r_friend_ids.count(aid) > 0;
                bool pending = r_pending_friend_ids.count(aid) > 0;

                float card_h = sc(56.f);
                ImVec2 cmin(cx, list_y);
                ImVec2 cmax(cx + cw, list_y + card_h);

                float hover_t = anim.Get(0x9000 + i, ImGui::IsMouseHoveringRect(cmin, cmax) ? 1.f : 0.f, 10.f);
                dl->AddRectFilled(cmin, cmax,
                    IM_COL32((int)(22 + hover_t * 6), (int)(21 + hover_t * 5), (int)(32 + hover_t * 8), 255), sc(8.f));
                dl->AddRect(cmin, cmax,
                    IM_COL32((int)(35 + hover_t * 30), 33, (int)(52 + hover_t * 50), 255), sc(8.f));

                float av_r = sc(15.f);
                ImVec2 av_c(cmin.x + sc(16.f) + av_r, list_y + card_h * 0.5f);

                ID3D11ShaderResourceView* sr_tex = nullptr;
                {
                    std::lock_guard<std::mutex> lk(r_friend_avatars_mtx);
                    auto it = r_friend_avatars.find(aid);
                    if (it != r_friend_avatars.end()) sr_tex = it->second.texture;
                }

                if (sr_tex)
                {
                    dl->AddImageRounded((ImTextureID)sr_tex,
                        ImVec2(av_c.x - av_r, av_c.y - av_r), ImVec2(av_c.x + av_r, av_c.y + av_r),
                        ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), av_r);
                }
                else
                {
                    dl->AddCircleFilled(av_c, av_r, AvatarColor(name));
                    char init[2] = { (char)toupper((unsigned char)name[0]), '\0' };
                    ImVec2 init_sz = ImGui::CalcTextSize(init);
                    dl->AddText(ImVec2(av_c.x - init_sz.x * 0.5f, av_c.y - init_sz.y * 0.5f),
                        IM_COL32(255, 255, 255, 255), init);
                }
                ImVec2 name_sz = ImGui::CalcTextSize(name);
                dl->AddText(ImVec2(av_c.x + av_r + sc(12.f), list_y + (card_h - name_sz.y) * 0.5f),
                    IM_COL32(218, 218, 235, 255), name);

                const char* btn_label = already_friend ? "Remove" : (pending ? "Cancel" : "Add");
                bool disabled = false;

                float btn_x = cmax.x - btn_w - sc(12.f);
                float btn_y = list_y + (card_h - btn_h) * 0.5f;
                ImVec2 bmin(btn_x, btn_y), bmax(btn_x + btn_w, btn_y + btn_h);

                float bt = 0.f;
                if (!disabled)
                {
                    ImGui::SetCursorPos(bmin);
                    char bid[16]; sprintf_s(bid, "##sa%d", i);
                    ImGui::InvisibleButton(bid, ImVec2(btn_w, btn_h));
                    bt = anim.Get(0x9100 + i, ImGui::IsItemHovered() ? 1.f : 0.f, 12.f);

                    if (ImGui::IsItemClicked())
                    {
                        if (already_friend)
                        {
                            api.RemoveFriend(aid);
                            toast.Push("Friend removed.", TOAST_INFO);
                        }
                        else if (pending)
                        {
                            r_pending_friend_ids.erase(aid);
                            api.RemoveFriend(aid);
                            toast.Push("Friend request cancelled.", TOAST_INFO);
                        }
                        else
                        {
                            r_pending_friend_ids.insert(aid);
                            api.AddFriend(aid);
                            toast.Push("Friend request sent!", TOAST_SUCCESS);
                        }
                    }
                }

                ImU32 bg_col = IM_COL32((int)(22 + bt * 10), (int)(21 + bt * 8), (int)(32 + bt * 16), 255);
                ImU32 border_col = (already_friend || pending)
                    ? IM_COL32((int)(35 + bt * 100), (int)(33 + bt * 2), (int)(52 + bt * 2), 255)
                    : IM_COL32((int)(35 + bt * 89), (int)(33 + bt * 25), (int)(52 + bt * 185), 255);
                ImU32 text_col = (already_friend || pending)
                    ? IM_COL32((int)(198 + bt * 40), (int)(198 - bt * 100), (int)(218 - bt * 100), 255)
                    : IM_COL32(198, 198, 218, 255);

                dl->AddRectFilled(bmin, bmax, bg_col, sc(6.f));
                dl->AddRect(bmin, bmax, border_col, sc(6.f));
                ImVec2 lsz = ImGui::CalcTextSize(btn_label);
                dl->AddText(ImVec2(bmin.x + (btn_w - lsz.x) * 0.5f, bmin.y + (btn_h - lsz.y) * 0.5f),
                    text_col, btn_label);

                list_y += card_h + sc(6.f);
            }
        }
    }
    else
    {
        if (r_friends.empty())
        {
            dl->AddText(ImVec2(cx, list_y), IM_COL32(60, 57, 80, 255), "No friends yet. Search for players to add them.");
        }
        else
        {
            dl->AddText(ImVec2(cx, list_y), IM_COL32(90, 90, 112, 255), "ALL FRIENDS");
            list_y += ImGui::CalcTextSize("ALL FRIENDS").y + sc(8.f);
            for (int i = 0; i < (int)r_friends.size(); i++)
            {
                DrawFriendCard(dl, cx, cw, list_y, r_friends[i], i);
                list_y += sc(56.f) + sc(6.f);
            }
        }
    }
}
