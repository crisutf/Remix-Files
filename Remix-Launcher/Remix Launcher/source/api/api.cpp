#include <source/api/api.hpp>
#include <source/ui/defines.hpp>
#include <source/utils/utils.hpp>
#include <source/vendor/json.hpp>
#include <wininet.h>
#include <windows.h>
#include <shlobj.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "bcrypt.lib")

#define BACKEND_BASE "https://remixogfn.dev"

static std::wstring GetSessionPath()
{
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return L"";
    std::wstring path = std::wstring(local) + L"\\Remix Launcher\\";
    CoTaskMemFree(local);
    CreateDirectoryW(path.c_str(), nullptr);
    path += L"session.json";
    return path;
}

static std::string HttpGet(const char* url)
{
    std::string result;
    HINTERNET hInternet = InternetOpenA("RemixLauncher/1.0", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hInternet) return result;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url, nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (hUrl)
    {
        char buf[4096];
        DWORD read = 0;
        while (InternetReadFile(hUrl, buf, sizeof(buf) - 1, &read) && read > 0)
        {
            buf[read] = '\0';
            result += buf;
            read = 0;
        }
        InternetCloseHandle(hUrl);
    }
    InternetCloseHandle(hInternet);
    return result;
}

static void HttpVerb(const char* method, const char* url)
{
    HINTERNET hInternet = InternetOpenA("RemixLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return;

    URL_COMPONENTSA uc = {};
    uc.dwStructSize = sizeof(uc);
    char scheme[16] = {}, host[256] = {}, path[1024] = {};
    uc.lpszScheme   = scheme; uc.dwSchemeLength   = sizeof(scheme);
    uc.lpszHostName = host;   uc.dwHostNameLength = sizeof(host);
    uc.lpszUrlPath  = path;   uc.dwUrlPathLength  = sizeof(path);
    InternetCrackUrlA(url, 0, 0, &uc);

    bool isHttps = (_stricmp(scheme, "https") == 0);
    INTERNET_PORT port = uc.nPort ? uc.nPort : (isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);

    HINTERNET hConn = InternetConnectA(hInternet, host, port,
        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (hConn)
    {
        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (isHttps) flags |= INTERNET_FLAG_SECURE;
        HINTERNET hReq = HttpOpenRequestA(hConn, method, path, nullptr, nullptr, nullptr, flags, 0);
        if (hReq)
        {
            HttpSendRequestA(hReq, nullptr, 0, nullptr, 0);
            InternetCloseHandle(hReq);
        }
        InternetCloseHandle(hConn);
    }
    InternetCloseHandle(hInternet);
}

static std::string HttpGetWithAuth(const char* url, const char* token)
{
    std::string result;
    HINTERNET hInternet = InternetOpenA("RemixLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return result;

    URL_COMPONENTSA uc = {};
    uc.dwStructSize = sizeof(uc);
    char scheme[16] = {}, host[256] = {}, path[1024] = {};
    uc.lpszScheme   = scheme; uc.dwSchemeLength   = sizeof(scheme);
    uc.lpszHostName = host;   uc.dwHostNameLength = sizeof(host);
    uc.lpszUrlPath  = path;   uc.dwUrlPathLength  = sizeof(path);
    InternetCrackUrlA(url, 0, 0, &uc);

    bool isHttps = (_stricmp(scheme, "https") == 0);
    INTERNET_PORT port = uc.nPort ? uc.nPort : (isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);

    HINTERNET hConn = InternetConnectA(hInternet, host, port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (hConn)
    {
        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (isHttps) flags |= INTERNET_FLAG_SECURE;
        HINTERNET hReq = HttpOpenRequestA(hConn, "GET", path, nullptr, nullptr, nullptr, flags, 0);
        if (hReq)
        {
            std::string hdr = std::string("Authorization: Bearer ") + token;
            HttpSendRequestA(hReq, hdr.c_str(), (DWORD)hdr.size(), nullptr, 0);

            DWORD status = 0, statusLen = sizeof(status);
            HttpQueryInfoA(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &statusLen, nullptr);

            if (status == 200)
            {
                char buf[4096]; DWORD read = 0;
                while (InternetReadFile(hReq, buf, sizeof(buf) - 1, &read) && read > 0)
                {
                    buf[read] = '\0';
                    result += buf;
                    read = 0;
                }
            }
            else if (status == 403)
            {
                result = "403";
            }
            InternetCloseHandle(hReq);
        }
        InternetCloseHandle(hConn);
    }
    InternetCloseHandle(hInternet);
    return result;
}

static std::vector<unsigned char> HttpGetBytes(const char* url)
{
    std::vector<unsigned char> result;
    HINTERNET hInternet = InternetOpenA("RemixLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return result;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url, nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (hUrl)
    {
        unsigned char buf[8192];
        DWORD read = 0;
        while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0)
        {
            result.insert(result.end(), buf, buf + read);
            read = 0;
        }
        InternetCloseHandle(hUrl);
    }
    InternetCloseHandle(hInternet);
    return result;
}


static std::string SHA256File(const std::wstring& path)
{
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return "";

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string result;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0)
    {
        DWORD hashLen = 0, cbData = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cbData, 0);

        if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0)
        {
            unsigned char buf[65536];
            DWORD read = 0;
            while (ReadFile(hFile, buf, sizeof(buf), &read, nullptr) && read > 0)
                BCryptHashData(hHash, buf, read, 0);

            std::vector<unsigned char> hash(hashLen);
            BCryptFinishHash(hHash, hash.data(), hashLen, 0);
            BCryptDestroyHash(hHash);

            const char* hex = "0123456789abcdef";
            for (auto b : hash) { result += hex[b >> 4]; result += hex[b & 0xf]; }
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
    CloseHandle(hFile);
    return result;
}

void c_api::FetchPlayerCount()
{
    std::thread([]() {
        std::string resp = HttpGet(BACKEND_BASE "/rmx/server/api/v1/clients");
        if (!resp.empty())
        {
            try { r_player_count = std::stoi(resp); }
            catch (...) { r_player_count = 0; }
        }
    }).detach();
}

void c_api::FetchProfile()
{

}

static std::string ExtractProfilePicture(const nlohmann::json& item)
{
    static const char* keys[] = {
        "profilePicture", "profile_picture",
        "profilePictureUrl", "profile_picture_url",
        "avatar", "avatarUrl", "avatar_url"
    };
    for (auto* k : keys)
    {
        if (item.contains(k) && item[k].is_string())
        {
            std::string v = item.value(k, "");
            if (!v.empty()) return v;
        }
    }
    return "";
}

static void QueueFriendAvatarFetch(const std::string& accountId, const std::string& url)
{
    if (accountId.empty() || url.empty()) return;

    {
        std::lock_guard<std::mutex> lk(r_friend_avatars_mtx);
        auto& av = r_friend_avatars[accountId];
        if (av.loadRequested && av.url == url && (av.texture != nullptr || av.pending)) return;
        av.url = url;
        av.loadRequested = true;
    }

    std::thread([accountId, url]() {
        auto bytes = HttpGetBytes(url.c_str());
        if (bytes.empty()) return;

        std::lock_guard<std::mutex> lk(r_friend_avatars_mtx);
        auto it = r_friend_avatars.find(accountId);
        if (it == r_friend_avatars.end() || it->second.url != url) return;
        it->second.pendingBytes = std::move(bytes);
        it->second.pending = true;
    }).detach();
}

void c_api::FetchFriends()
{
    if (r_account_id.empty()) return;
    std::thread([]() {
        std::string url = std::string(BACKEND_BASE) + "/rmx/server/api/v1/friends/" + r_account_id;
        std::string resp = HttpGet(url.c_str());
        if (resp.empty()) return;

        try
        {
            auto j = nlohmann::json::parse(resp);
            std::vector<FriendEntry> friends;
            std::unordered_set<std::string> ids;
            for (auto& item : j)
            {
                FriendEntry e;
                e.accountId = item.value("accountId",   "");
                e.displayName = item.value("displayName", "");
                e.profilePictureUrl = ExtractProfilePicture(item);
                e.online = item.value("online", false);
                e.status = item.value("status", "");
                if (!e.accountId.empty())
                {
                    friends.push_back(e);
                    ids.insert(e.accountId);
                    QueueFriendAvatarFetch(e.accountId, e.profilePictureUrl);
                }
            }
            r_friends = std::move(friends);
            r_friend_ids = std::move(ids);
        }
        catch (...) {}
        
        std::string pendingUrl = std::string(BACKEND_BASE) + "/friends/api/v1/" + r_account_id + "/summary";
        std::string pendingResp = HttpGet(pendingUrl.c_str());
        if (!pendingResp.empty())
        {
            try
            {
                auto pj = nlohmann::json::parse(pendingResp);
                std::unordered_set<std::string> pending;
                if (pj.contains("outgoing") && pj["outgoing"].is_array())
                    for (auto& item : pj["outgoing"])
                    {
                        std::string aid = item.value("accountId", "");
                        if (!aid.empty()) pending.insert(aid);
                    }
                r_pending_friend_ids = std::move(pending);
            }
            catch (...) {}
        }
    }).detach();
}

void c_api::AddFriend(const std::string& friendAccountId)
{
    if (r_account_id.empty() || friendAccountId.empty()) return;
    std::string url = std::string(BACKEND_BASE) + "/friends/api/v1/" + r_account_id + "/friends/" + friendAccountId;
    std::thread([url]() { HttpVerb("POST", url.c_str()); }).detach();
}

void c_api::RemoveFriend(const std::string& friendAccountId)
{
    if (r_account_id.empty() || friendAccountId.empty()) return;
    std::string url = std::string(BACKEND_BASE) + "/friends/api/v1/" + r_account_id + "/friends/" + friendAccountId;
    std::thread([url]() { HttpVerb("DELETE", url.c_str()); }).detach();

    r_friend_ids.erase(friendAccountId);
    r_friends.erase(std::remove_if(r_friends.begin(), r_friends.end(),
        [&](const FriendEntry& f) { return f.accountId == friendAccountId; }), r_friends.end());
}

void c_api::SearchPlayers(const char* query)
{
    r_search_results.clear();
    r_searching = false;
    if (!query || !query[0]) return;

    std::string q = query;
    std::string accId = r_account_id.empty() ? "0" : r_account_id;
    std::thread([q, accId]() {
        std::string url = std::string(BACKEND_BASE) + "/api/v1/search/" + accId + "?prefix=" + q + "&platform=default";
        std::string resp = HttpGet(url.c_str());
        if (resp.empty()) return;

        try
        {
            auto j = nlohmann::json::parse(resp);
            std::vector<SearchResult> results;
            for (auto& item : j)
            {
                SearchResult sr;
                sr.accountId = item.value("accountId", "");
                auto& matches = item["matches"];
                if (!matches.empty())
                    sr.name = matches[0].value("value", "");
                sr.profilePictureUrl = item.value("profile_picture", "");
                if (!sr.accountId.empty() && !sr.name.empty() && sr.accountId != r_account_id)
                {
                    results.push_back(sr);
                    QueueFriendAvatarFetch(sr.accountId, sr.profilePictureUrl);
                }
            }
            r_search_results = std::move(results);
        }
        catch (...) {}
        r_searching = false;
    }).detach();
    r_searching = true;
}

void c_api::SaveSession()
{
    std::wstring path = GetSessionPath();
    if (path.empty()) return;

    nlohmann::json j;
    j["token"]           = r_auth_token;
    j["account_id"]      = r_account_id;
    j["display_name"]    = r_account_name;
    j["profile_picture"] = r_profile_picture_url;
    j["role_name"]  = r_account_role;
    j["role_color"] = (unsigned int)r_account_role_color;

    std::ofstream f(path);
    if (f) f << j.dump();
}

void c_api::ClearSession()
{
    std::wstring path = GetSessionPath();
    if (!path.empty()) DeleteFileW(path.c_str());
}

void c_api::RestoreSession()
{
    std::thread([]() {
        std::wstring path = GetSessionPath();
        if (path.empty()) return;

        std::ifstream f(path);
        if (!f) return;

        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (content.empty()) return;

        try
        {
            auto j = nlohmann::json::parse(content);
            std::string token = j.value("token", "");
            if (token.empty()) return;

            r_auth_token = token;
            r_account_id = j.value("account_id", "");
            r_account_name = j.value("display_name", "");
            r_profile_picture_url = j.value("profile_picture", "");
            r_account_role = j.value("role_name", "");
            r_account_role_color  = (ImU32)j.value("role_color", (unsigned int)IM_COL32(90, 90, 112, 255));

            if (!r_profile_picture_url.empty())
            {
                auto bytes = HttpGetBytes(r_profile_picture_url.c_str());
                if (!bytes.empty())
                {
                    r_pending_profile_bytes = std::move(bytes);
                    r_profile_picture_pending = true;
                }
            }

            r_auth_done = true;

            api.FetchFriends();
            api.FetchPlayerCount();

            std::thread([]() {
                while (r_auth_done)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(60));
                    if (r_auth_done) { api.FetchFriends(); api.FetchPlayerCount(); }
                }
            }).detach();
        }
        catch (...) {}
    }).detach();
}

static std::atomic<int> s_discord_seq{ 0 };

void c_api::LoginWithDiscord()
{
    std::string state;
    {
        srand((unsigned)GetTickCount());
        char buf[17];
        const char* hex = "0123456789abcdef";
        for (int i = 0; i < 16; i++) buf[i] = hex[rand() % 16];
        buf[16] = '\0';
        state = buf;
    }

    int seq = ++s_discord_seq;
    r_discord_state = state;
    r_discord_polling = true;

    std::string authUrl = std::string(BACKEND_BASE) + "/rmx/server/api/v1/discord/auth?state=" + state;
    ShellExecuteA(nullptr, "open", authUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    std::thread([state, seq]() {
        std::string pollUrl = std::string(BACKEND_BASE) + "/rmx/server/api/v1/discord/pending/" + state;

        for (int i = 0; i < 45; i++)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));

            if (s_discord_seq != seq) return;
            std::string resp = HttpGet(pollUrl.c_str());
            if (resp.empty()) continue;
            if (resp.find("\"pending\":true") != std::string::npos) continue;

            try
            {
                auto j = nlohmann::json::parse(resp);
                std::string token = j.value("token", "");
                if (token.empty()) continue;

                auto& user = j["user"];
                r_account_id = user.value("id", "");
                r_account_name = user.value("display_name", "Player");
                r_auth_token = token;

                r_account_role = user.value("role_name", "");
                int dc = user.value("role_color", 0);
                if (dc != 0)
                    r_account_role_color = IM_COL32((dc >> 16) & 0xFF, (dc >> 8) & 0xFF, dc & 0xFF, 255);
                else
                    r_account_role_color = IM_COL32(90, 90, 112, 255);

                r_profile_picture_url = user.value("profile_picture", "");
                if (!r_profile_picture_url.empty())
                {
                    auto bytes = HttpGetBytes(r_profile_picture_url.c_str());
                    if (!bytes.empty())
                    {
                        r_pending_profile_bytes = std::move(bytes);
                        r_profile_picture_pending = true;
                    }
                }
            }
            catch (...) { continue; }

            r_discord_polling = false;
            r_auth_done = true;

            api.SaveSession();
            api.FetchFriends();
            api.FetchPlayerCount();

            std::thread([]() {
                while (r_auth_done)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                    if (r_auth_done)
                    {
                        api.FetchFriends();
                        api.FetchPlayerCount();
                    }
                }
            }).detach();

            break;
        }

        if (s_discord_seq == seq)
            r_discord_polling = false;
    }).detach();
}

void c_api::LoginWithArc(const std::string& iss, const std::string& sub, const std::string& dn)
{
    std::string p_iss = iss;
    std::string p_sub = sub;
    std::string p_dn = dn;

    std::thread([p_iss, p_sub, p_dn]() {
        try
        {
            if (!r_arc_instance)
            {
                ArcConfiguration cfg;
                cfg.ClientID = "rokzoyzmxxjqekqdqkipgkdueoybqjqo";
                cfg.stage = ArcStage::Dev;
                r_arc_instance = new ArcInstance(cfg);
            }

            std::string identity = r_arc_instance->CreateIdentity(p_iss, p_sub, p_dn);
            ArcSession session = r_arc_instance->CreateAuthSession(identity);

            r_auth_token = session.auth.token;
            r_account_id = session.auth.clid;
            r_account_name = session.account.display_name;

            r_auth_done = true;

            api.SaveSession();
            api.FetchFriends();
            api.FetchPlayerCount();

            std::thread([]() {
                while (r_auth_done)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    if (r_auth_done) { api.FetchFriends(); api.FetchPlayerCount(); }
                }
            }).detach();
        }
        catch (...) {}
    }).detach();
}
