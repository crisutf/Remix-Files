#include <source/discord/rpc.hpp>
#include <source/ui/defines.hpp>
#include <source/launch/launch_game.hpp>
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdio>

static std::string s_app_id;
static std::atomic<bool> s_running{ false };
static std::thread s_thread;
static int64_t s_start_time = 0;

static bool WriteMsg(HANDLE h, uint32_t op, const char* json)
{
    uint32_t len = (uint32_t)strlen(json);
    DWORD w;
    return WriteFile(h, &op, 4, &w, nullptr) && w == 4
        && WriteFile(h, &len, 4, &w, nullptr) && w == 4
        && WriteFile(h, json, len, &w, nullptr) && w == len;
}

static void ReadDrain(HANDLE h)
{
    DWORD avail = 0;
    if (!PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr) || avail < 8) return;
    uint32_t op, len; DWORD r;
    if (!ReadFile(h, &op,  4, &r, nullptr) || r != 4) return;
    if (!ReadFile(h, &len, 4, &r, nullptr) || r != 4) return;
    if (len > 0)
    {
        std::string buf(len, '\0');
        ReadFile(h, &buf[0], len, &r, nullptr);
    }
}

static HANDLE TryConnect()
{
    for (int i = 0; i < 10; i++)
    {
        char path[64];
        snprintf(path, sizeof(path), "\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) return h;
    }
    return INVALID_HANDLE_VALUE;
}

static std::string Activity()
{
    const char* state = "In the launcher";
    if (r_launch_state == LaunchState::Launching) state = "Launching game";
    else if (r_launch_state == LaunchState::Running)   state = "Playing Remix";

    char buf[1024];
    if (!r_account_name.empty())
    {
        char logged_in_txt[256];
        snprintf(logged_in_txt, sizeof(logged_in_txt), "Logged in as %s", r_account_name.c_str());
        snprintf(buf, sizeof(buf),
            "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,\"activity\":{"
            "\"details\":\"%s\","
            "\"state\":\"%s\","
            "\"timestamps\":{\"start\":%lld},"
            "\"buttons\":[{\"label\":\"Join the Discord\",\"url\":\"https://discord.gg/remixfn\"}]"
            "}},\"nonce\":\"rpc1\"}",
            GetCurrentProcessId(), state, logged_in_txt, (long long)s_start_time);
    }
    else
    {
        snprintf(buf, sizeof(buf),
            "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,\"activity\":{"
            "\"details\":\"%s\","
            "\"timestamps\":{\"start\":%lld},"
            "\"buttons\":[{\"label\":\"Join the Discord\",\"url\":\"https://discord.gg/remixfn\"}]"
            "}},\"nonce\":\"rpc1\"}",
            GetCurrentProcessId(), state, (long long)s_start_time);
    }
    return buf;
}

static void SleepCheck(int ms)
{
    int steps = ms / 50;
    for (int i = 0; i < steps && s_running; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

static void RpcThread()
{
    while (s_running)
    {
        HANDLE h = TryConnect();
        if (h == INVALID_HANDLE_VALUE) { SleepCheck(5000); continue; }

        char hs[128];
        snprintf(hs, sizeof(hs), "{\"v\":1,\"client_id\":\"%s\"}", s_app_id.c_str());
        if (!WriteMsg(h, 0, hs)) { CloseHandle(h); SleepCheck(5000); continue; }

        bool ready = false;
        for (int i = 0; i < 50 && s_running; i++)
        {
            DWORD avail = 0;
            PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr);
            if (avail >= 8) { ReadDrain(h); ready = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!ready || !s_running) { CloseHandle(h); continue; }

        std::string last = Activity();
        if (!WriteMsg(h, 1, last.c_str())) { CloseHandle(h); SleepCheck(5000); continue; }
        SleepCheck(500);
        ReadDrain(h);

        bool alive = true;
        while (s_running && alive)
        {
            SleepCheck(3000);
            if (!s_running) break;

            std::string cur = Activity();
            if (cur != last)
            {
                last = cur;
                if (!WriteMsg(h, 1, last.c_str())) { alive = false; break; }
                SleepCheck(300);
                ReadDrain(h);
            }
        }

        CloseHandle(h);
        if (s_running) SleepCheck(3000);
    }
}

void c_discord_rpc::Init(const char* appId)
{
    s_app_id = appId;
    s_start_time = (int64_t)std::time(nullptr);
    s_running = true;
    s_thread = std::thread(RpcThread);
}

void c_discord_rpc::Shutdown()
{
    s_running = false;
    if (s_thread.joinable()) s_thread.join();
}

void c_discord_rpc::Update() {}
