#include <filesystem>
#include <fstream>
#include <map>
#include <source/includes.hpp>
#include <source/ui/pages/include/downloads.hpp>
#include <source/utils/utils.hpp>
#include <source/vendor/json.hpp>
#include <source/vendor/sha1.h>
#include <thread>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

struct DownloadEntry
{
    const char* version;
    const char* cl;
    const char* season;
    const char* manifest;
    const wchar_t* image_file;
    ID3D11ShaderResourceView* texture = nullptr;
    int tex_w = 0, tex_h = 0;
    bool loaded = false;
    bool downloading = false;
    float progress = 0.f;
    int numChunksRemaining = 0;
    float speed = 0.f;
    std::string dest_folder;
};

struct Info
{
    std::string Name;
    size_t FileOffset;
    size_t Start;
    size_t End;
};

struct Manifest
{
    size_t Size;
    std::vector<std::string> Empties;
    std::map<std::string, std::vector<Info>> ChunkInfos;
};

void from_json(const nlohmann::json& j, Info& info)
{
    j.at("name").get_to(info.Name);
    j.at("offset").get_to(info.FileOffset);
    j.at("start").get_to(info.Start);
    j.at("end").get_to(info.End);
}

void from_json(const nlohmann::json& j, Manifest& manifest)
{
    j.at("size").get_to(manifest.Size);
    j.at("empty").get_to(manifest.Empties);
    j.at("chunks").get_to(manifest.ChunkInfos);
}

static DownloadEntry s_entries[] = {
    { "32.11", "CL-38202817", "Chapter 2 Remix", "cfb8d3111ce911635280c1bc6e1bd1de2fc0e2f0", L"splash.png", nullptr, 0, 0, false, false, 0.f, 0, 0.f, "" },
};

float GetDownloadProgress(int index)
{
    if (index < 0 || index >= (int)(sizeof(s_entries) / sizeof(s_entries[0])))
        return 0.f;
    return s_entries[index].progress;
}

static void FormatETA(char* buf, size_t sz, float seconds)
{
    if (seconds <= 0.f || seconds > 360000.f)
    {
        sprintf_s(buf, sz, "calculating...");
        return;
    }
    int s = (int)seconds;
    int h = s / 3600;
    s %= 3600;
    int m = s / 60;
    s %= 60;
    if (h > 0)
        sprintf_s(buf, sz, "%dh %dm %ds", h, m, s);
    else if (m > 0)
        sprintf_s(buf, sz, "%dm %ds", m, s);
    else
        sprintf_s(buf, sz, "%ds", s);
}

unsigned long long bytesDownloaded = 0, totalBytesDownloaded = 0, bdls = 0, ls = 0;

unsigned char* DownloadToBuffer(const char* url, size_t* outSize = nullptr)
{
    HINTERNET hInternet = NULL, hConnect = NULL;
    unsigned char* buffer = NULL;
    size_t totalBytesRead = 0;
    size_t bufferSize = 0;
    const size_t CHUNK_SIZE = 1024768;

    hInternet = InternetOpenA("RemixLauncher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet)
        return NULL;

    hConnect = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect)
    {
        InternetCloseHandle(hInternet);
        return NULL;
    }

    auto tempChunk = malloc(CHUNK_SIZE);
    DWORD bytesRead = 0;

    while (InternetReadFile(hConnect, tempChunk, CHUNK_SIZE, &bytesRead) && bytesRead > 0)
    {
        bytesDownloaded += bytesRead;
        totalBytesDownloaded += bytesRead;

        unsigned char* newBuffer = (unsigned char*)realloc(buffer, totalBytesRead + bytesRead);
        if (!newBuffer)
        {
            free(buffer);
            totalBytesRead = 0;
            break;
        }

        buffer = newBuffer;

        memcpy(buffer + totalBytesRead, tempChunk, bytesRead);
        totalBytesRead += bytesRead;
    }

    free(tempChunk);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    if (outSize)
        *outSize = totalBytesRead;
    return buffer;
}

#define BASE_URL "https://r2.ploosh.dev"

unsigned char* downloadChunk(DownloadEntry* entry, std::string ChunkHash)
{
    auto chunkURL = std::string(BASE_URL "/chunks/") + entry->manifest + "/" + ChunkHash + ".chunk";
    size_t chunkSize = 0;
    auto chunkMem = DownloadToBuffer(chunkURL.c_str(), &chunkSize);

    if (!chunkMem)
        chunkMem = DownloadToBuffer(chunkURL.c_str(), &chunkSize);

    sha1::SHA1 sh;
    sh.processBytes(chunkMem, chunkSize);
    uint32_t digest[5];
    sh.getDigest(digest);

    char tmp[48];
    snprintf(tmp, 45, "%08x%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3], digest[4]);

    if (ChunkHash != tmp)
    {
        free(chunkMem);
        return downloadChunk(entry, ChunkHash);
    }
    else
        return chunkMem;
};

int numThreads = 0;
int i = 0;
long long lastsp = 0;
int numChunks = 0;
std::vector<float> speeds;
std::vector<std::string> CompletedChunks;
unsigned long long manifestBytes;

void DownloaderThread(DownloadEntry* entry, std::string ChunkHash, std::vector<Info> Files, std::filesystem::path DownloadFolder)
{
    numThreads++;

    auto t1 = std::chrono::high_resolution_clock::now();
    auto chunkMem = downloadChunk(entry, ChunkHash);

    for (auto& file : Files)
    {
        auto path = DownloadFolder / file.Name;
        auto size = file.End - file.Start;
        std::filesystem::create_directories(path.parent_path());

        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        while (hFile == INVALID_HANDLE_VALUE) // another thr is already operating on it
        {
            Sleep(25);
            hFile = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }

        if (hFile != INVALID_HANDLE_VALUE)
        {
            LARGE_INTEGER liOffset;
            liOffset.QuadPart = file.FileOffset;

            if (SetFilePointerEx(hFile, liOffset, NULL, FILE_BEGIN))
            {
                DWORD bytesWritten = 0;
                WriteFile(hFile, chunkMem + file.Start, (DWORD)size, &bytesWritten, NULL);
            }
        }

        CloseHandle(hFile);
    }
    CompletedChunks.push_back(ChunkHash);

    auto j = nlohmann::json(CompletedChunks).dump();

    std::ofstream rmxf(DownloadFolder / ".rmx");

    rmxf.write(j.c_str(), j.size());

    rmxf.close();

    free(chunkMem);

    auto t2 = std::chrono::high_resolution_clock::now();
    i++;
    entry->progress = ((float)i / numChunks) * 100.f;
    entry->numChunksRemaining = numChunks - i;

    auto d = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count();

    if (d != lastsp)
        entry->speed = (float)(d > lastsp ? d - lastsp : d);
    lastsp = d;

    if (speeds.size() >= 10u)
        speeds.erase(speeds.begin());
    speeds.push_back(entry->speed);

    numThreads--;
}

void DownloadThread(std::string DownloadFolderStr, DownloadEntry* entry)
{
    bytesDownloaded = 0;
    bdls = 0;

    std::filesystem::path BaseDownloadFolder = DownloadFolderStr;
    std::filesystem::path DownloadFolder = BaseDownloadFolder / "32.11";

    auto manifestURL = std::string(BASE_URL "/manifests/") + entry->manifest + ".manifest";
    auto manifestData = DownloadToBuffer(manifestURL.c_str());

    auto manifest = nlohmann::json::parse(manifestData).get<Manifest>();
    free(manifestData);

    manifestBytes = manifest.Size;

    std::filesystem::create_directories(DownloadFolder);

    for (auto& Empty : manifest.Empties)
    {
        auto EmptyFile = DownloadFolder / Empty;

        std::filesystem::create_directories(EmptyFile.parent_path());

        std::ofstream(EmptyFile).close();
    }

    unsigned int threads = std::thread::hardware_concurrency();

    if (threads == 0)
        threads = 2;
    else
        threads = min(threads / 2, 4);

    std::ifstream rmxf(DownloadFolder / ".rmx", std::ios::ate);

    i = 0;
    numChunks = (int)manifest.ChunkInfos.size();
    if (rmxf.good())
    {
        auto size = (long long)rmxf.tellg();
        auto brother = (char*)malloc(size + 1);

        rmxf.seekg(0);
        rmxf.read(brother, size);
        brother[size] = 0;

        auto rmxd = nlohmann::json::parse(brother).get<std::vector<std::string>>();
        free(brother);

        for (auto& Chunk : rmxd)
            if (manifest.ChunkInfos.erase(Chunk))
            {
                totalBytesDownloaded += 128 * 1024 * 1024;
                CompletedChunks.push_back(Chunk);
                i++;
            }

        rmxf.close();

        entry->progress = ((float)i / numChunks) * 100.f;
    }

    ls = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    for (auto& [ChunkHash, Files] : manifest.ChunkInfos)
    {
        while ((unsigned int)numThreads > threads)
            Sleep(10);

        std::thread(DownloaderThread, entry, ChunkHash, Files, DownloadFolder).detach();
    }

    while (numThreads > 0)
        Sleep(50);

    std::filesystem::remove(DownloadFolder / ".rmx");

    r_pending_add_build_path = DownloadFolder.string();
    r_pending_add_build = true;
    entry->downloading = false;
}

void c_downloads::Draw(ImDrawList* dl, float cx, float cw, float hy)
{
    if (r_pending_add_build)
    {
        r_pending_add_build = false;
        std::string path = r_pending_add_build_path;
        r_pending_add_build_path = "";
        utils.AddBuild(path);
        toast.Push("Build installed! Check your Library.", TOAST_SUCCESS);
    }

    dl->AddText(ImVec2(cx, hy + sc(7.f)), IM_COL32(255, 255, 255, 255), "Available Downloads");

    float divider_y = r_titlebar_height + sc(62.f);
    dl->AddLine(ImVec2(r_sidebar_width, divider_y), ImVec2(r_width, divider_y), IM_COL32(35, 33, 52, 255));

    float img_w = sc(195.f);
    float img_h = sc(215.f);
    float info_h = sc(72.f);
    float card_w = img_w;
    float card_h = img_h + info_h;
    float gap = sc(18.f);
    float pad_x = sc(6.f);
    float start_y = divider_y + sc(20.f);

    double now = ImGui::GetTime();
    int count = sizeof(s_entries) / sizeof(s_entries[0]);

    for (int i = 0; i < count; i++)
    {
        auto& entry = s_entries[i];

        if (!entry.loaded)
        {
            std::wstring img_path = utils.GetAssetsDir() + entry.image_file;
            entry.texture = utils.LoadImageFromFile(img_path.c_str(), &entry.tex_w, &entry.tex_h);
            entry.loaded = true;
        }

        if (!entry.downloading)
        {
            bool in_library = false;
            for (auto& b : r_builds)
            {
                bool path_match = !entry.dest_folder.empty() && b.path == entry.dest_folder;
                bool ver_match = !b.version.empty() && b.version.find(entry.version) != std::string::npos;
                if (path_match || ver_match)
                {
                    in_library = true;
                    break;
                }
            }
            if (in_library)
                entry.progress = 100.f;
            else if (entry.progress >= 100.f)
                entry.progress = 0.f;
        }

        bool has_partial = !entry.dest_folder.empty() && std::filesystem::exists(std::filesystem::path(entry.dest_folder) / ".rmx");
        bool show_bar = entry.downloading || (entry.progress > 0.f && entry.progress < 100.f);
        bool completed = entry.progress >= 100.f;
        bool resumable = !completed && !entry.downloading && has_partial;

        float enter_t = anim.Get(0x8000 + i, 1.f, 10.f + i * 1.5f);
        int ialpha = (int)(enter_t * 255.f);

        float card_x = cx + pad_x + i * (card_w + gap);
        float card_y = start_y + (1.f - enter_t) * sc(16.f);

        ImVec2 card_min(card_x, card_y);
        ImVec2 card_max(card_x + card_w, card_y + card_h);
        ImVec2 img_min(card_x, card_y);
        ImVec2 img_max(card_x + img_w, card_y + img_h);

        bool img_hov = !completed && !show_bar && ImGui::IsMouseHoveringRect(img_min, img_max);
        float hover_t = anim.Get(0x8100 + i, img_hov ? 1.f : 0.f, 10.f);

        dl->AddRectFilled(card_min, card_max, IM_COL32(22, 21, 32, ialpha), sc(10.f));
        dl->AddRect(card_min, card_max, IM_COL32(35, 33, 52, ialpha), sc(10.f));

        if (entry.texture)
            dl->AddImageRounded((ImTextureID)entry.texture, img_min, img_max, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), IM_COL32(255, 255, 255, ialpha), sc(10.f));
        else
            dl->AddRectFilled(img_min, img_max, IM_COL32(30, 28, 45, ialpha), sc(10.f));

        dl->AddRectFilledMultiColor(ImVec2(img_min.x, img_max.y - sc(70.f)), img_max, IM_COL32(22, 21, 32, 0), IM_COL32(22, 21, 32, 0), IM_COL32(22, 21, 32, (int)(210 * enter_t)),
            IM_COL32(22, 21, 32, (int)(210 * enter_t)));

        if (hover_t > 0.001f)
            dl->AddRectFilled(img_min, img_max, IM_COL32(0, 0, 0, (int)(hover_t * 110.f)), sc(10.f));

        if (!completed && !show_bar)
        {
            ImVec2 btn_c(card_x + card_w * 0.5f, card_y + img_h * 0.5f);
            float btn_r = sc(26.f);

            dl->AddCircleFilled(btn_c, btn_r, IM_COL32(20, 18, 30, (int)(160 * enter_t)));
            dl->AddCircle(btn_c, btn_r, IM_COL32(80, 70, 120, (int)(140 * enter_t)), 32, 1.f);

            if (r_font_icon)
            {
                const char* dl_icon = resumable ? "\xef\x81\xb8" : "\xef\x80\x99";
                float icon_sz = sc(15.f);
                ImVec2 icon_dim = r_font_icon->CalcTextSizeA(icon_sz, FLT_MAX, 0.f, dl_icon);
                dl->AddText(r_font_icon, icon_sz, ImVec2(btn_c.x - icon_dim.x * 0.5f, btn_c.y - icon_dim.y * 0.5f), IM_COL32(255, 255, 255, ialpha), dl_icon);
            }

            if (resumable)
            {
                const char* resume_label = "Resume";
                ImVec2 lbl_sz = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.f, resume_label);
                dl->AddText(ImVec2(btn_c.x - lbl_sz.x * 0.5f, btn_c.y + sc(30.f)), IM_COL32(180, 180, 210, ialpha), resume_label);
            }

            ImGui::SetCursorScreenPos(img_min);
            char btn_id[24];
            sprintf_s(btn_id, "##dlcard%d", i);
            ImGui::InvisibleButton(btn_id, ImVec2(card_w, img_h));
            if (ImGui::IsItemClicked())
            {
                if (resumable)
                {
                    entry.downloading = true;
                    entry.progress = 0.f;
                    entry.speed = 0.f;
                    std::thread(DownloadThread, entry.dest_folder, &entry).detach();
                }
                else
                {
                    std::string folder = utils.BrowseFolder();
                    if (!folder.empty())
                    {
                        entry.dest_folder = folder;
                        entry.downloading = true;
                        entry.progress = 0.f;
                        entry.speed = 0.f;
                        std::thread(DownloadThread, folder, &entry).detach();
                    }
                }
            }
        }

        if (completed)
        {
            float badge_x = card_x + sc(8.f);
            float badge_y = card_y + sc(8.f);
            dl->AddRectFilled(ImVec2(badge_x, badge_y), ImVec2(badge_x + sc(64.f), badge_y + sc(22.f)), IM_COL32(44, 182, 108, (int)(200 * enter_t)), sc(5.f));
            dl->AddText(ImVec2(badge_x + sc(7.f), badge_y + sc(4.f)), IM_COL32(255, 255, 255, ialpha), "Installed");
        }

        if (show_bar)
        {
            float smooth_p = anim.Get(0x8300 + i, entry.progress / 100.f, 8.f);
            float fill_w = img_w * smooth_p;

            float overlay_y = card_y + img_h * 0.3f;
            dl->AddRectFilledMultiColor(ImVec2(card_x, overlay_y), img_max, IM_COL32(12, 11, 20, 0), IM_COL32(12, 11, 20, 0), IM_COL32(12, 11, 20, (int)(230 * enter_t)),
                IM_COL32(12, 11, 20, (int)(230 * enter_t)));

            float bar_y = img_max.y - sc(6.f);
            float bar_h = sc(4.f);
            dl->AddRectFilled(ImVec2(card_x, bar_y), ImVec2(card_x + img_w, bar_y + bar_h), IM_COL32(16, 15, 24, ialpha));

            if (fill_w > 0.f)
            {
                dl->PushClipRect(ImVec2(card_x, bar_y), ImVec2(card_x + fill_w, bar_y + bar_h), true);
                dl->AddRectFilledMultiColor(ImVec2(card_x, bar_y), ImVec2(card_x + img_w, bar_y + bar_h), IM_COL32(100, 40, 210, ialpha), IM_COL32(150, 90, 250, ialpha),
                    IM_COL32(150, 90, 250, ialpha), IM_COL32(100, 40, 210, ialpha));

                if (fill_w > sc(4.f))
                {
                    float shimmer = fmodf((float)now * 120.f, fill_w + sc(60.f)) - sc(30.f);
                    float sx1 = card_x + shimmer, sx2 = sx1 + sc(30.f);
                    if (sx1 < card_x)
                        sx1 = card_x;
                    if (sx2 > card_x + fill_w)
                        sx2 = card_x + fill_w;
                    if (sx2 > sx1)
                        dl->AddRectFilled(ImVec2(sx1, bar_y), ImVec2(sx2, bar_y + bar_h), IM_COL32(255, 255, 255, 22));
                }
                dl->PopClipRect();
            }

            int pct_int = (int)entry.progress;
            char pct_buf[16];
            sprintf_s(pct_buf, "%d%%", pct_int);
            ImVec2 pct_size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.f, pct_buf);

            auto cs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            if (ls != 0 && cs != ls)
            {
                bdls = bytesDownloaded;
                bytesDownloaded = 0;
                ls = cs;
            }

            char downloadedStr[32];
            float mbDownloaded = (float)totalBytesDownloaded / (1024.f * 1024.f);

            if (mbDownloaded >= (1024.f * 1024.f))
                sprintf_s(downloadedStr, "%.1f TB", mbDownloaded / (1024.f * 1024.f));
            else if (mbDownloaded >= 1024.f)
                sprintf_s(downloadedStr, "%.1f GB", mbDownloaded / 1024.f);
            else if (mbDownloaded >= 1.f)
                sprintf_s(downloadedStr, "%.1f MB", mbDownloaded);
            else
                sprintf_s(downloadedStr, "%.0f KB", mbDownloaded * 1024.f);

            char sizeStr[32];
            float totalMB = (float)manifestBytes / (1024.f * 1024.f);

            if (totalMB >= (1024.f * 1024.f))
                sprintf_s(sizeStr, "%.1f TB", totalMB / (1024.f * 1024.f));
            else if (totalMB >= 1024.f)
                sprintf_s(sizeStr, "%.1f GB", totalMB / 1024.f);
            else if (totalMB >= 1.f)
                sprintf_s(sizeStr, "%.1f MB", totalMB);
            else
                sprintf_s(sizeStr, "%.0f KB", totalMB * 1024.f);

            char info_line1[100] = {};
            char info_line2[100] = {};
            sprintf_s(info_line1, "%s/%s", downloadedStr, sizeStr);
            if (entry.speed > 0.01f && !speeds.empty())
            {
                float eta_sec = (float)entry.numChunksRemaining * (std::accumulate(speeds.begin(), speeds.end(), 0.f) / (float)speeds.size());
                char eta_buf[32];
                FormatETA(eta_buf, sizeof(eta_buf), eta_sec);
                if (bdls)
                {
                    float mbs = (float)bdls / (1024.f * 1024.f);
                    char spd[24];
                    if (mbs >= 1024.f)
                        sprintf_s(spd, "%.1f GB/s", mbs / 1024.f);
                    else if (mbs >= 1.f)
                        sprintf_s(spd, "%.1f MB/s", mbs);
                    else
                        sprintf_s(spd, "%.0f KB/s", mbs * 1024.f);
                    sprintf_s(info_line2, "%s  |  ETA: %s", spd, eta_buf);
                }
                else
                    sprintf_s(info_line2, "ETA: %s", eta_buf);
            }
            else
                sprintf_s(info_line2, "ETA: calculating...");

            ImVec2 info_size1 = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.f, info_line1);
            ImVec2 info_size2 = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.f, info_line2);
            float info_max_w = max(info_size1.x, info_size2.x);
            float line_h = info_size1.y;

            float pill_pad_x = sc(12.f);
            float pill_pad_y = sc(8.f);
            float pill_gap = sc(6.f);
            float line_gap = sc(2.f);
            float pill_w = max(pct_size.x, info_max_w) + pill_pad_x * 2.f;
            float pill_h = pct_size.y + line_h * 2.f + line_gap + pill_pad_y * 2.f + pill_gap;
            float pill_x = card_x + (img_w - pill_w) * 0.5f;
            float pill_y = card_y + img_h * 0.5f - pill_h * 0.5f;

            dl->AddRectFilled(ImVec2(pill_x, pill_y), ImVec2(pill_x + pill_w, pill_y + pill_h), IM_COL32(10, 9, 18, (int)(220 * enter_t)), sc(8.f));
            dl->AddRect(ImVec2(pill_x, pill_y), ImVec2(pill_x + pill_w, pill_y + pill_h), IM_COL32(60, 50, 90, (int)(180 * enter_t)), sc(8.f));

            dl->AddText(ImVec2(pill_x + (pill_w - pct_size.x) * 0.5f, pill_y + pill_pad_y), IM_COL32(255, 255, 255, ialpha), pct_buf);
            float info_y = pill_y + pill_pad_y + pct_size.y + pill_gap;
            dl->AddText(ImVec2(pill_x + (pill_w - info_size1.x) * 0.5f, info_y), IM_COL32(160, 150, 200, ialpha), info_line1);
            dl->AddText(ImVec2(pill_x + (pill_w - info_size2.x) * 0.5f, info_y + line_h + line_gap), IM_COL32(160, 150, 200, ialpha), info_line2);
        }

        float lbl_x = card_x + sc(12.f);
        float lbl_y = card_y + img_h + sc(10.f);
        char ver_buf[64];
        sprintf_s(ver_buf, "Fortnite %s", entry.version);
        dl->AddText(ImVec2(lbl_x, lbl_y), IM_COL32(224, 224, 240, ialpha), ver_buf);
        dl->AddText(ImVec2(lbl_x, lbl_y + sc(20.f)), IM_COL32(90, 90, 112, ialpha), entry.cl);
    }
}
