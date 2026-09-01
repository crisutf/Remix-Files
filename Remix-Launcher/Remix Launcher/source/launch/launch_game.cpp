#include <source/launch/launch_game.hpp>
#include <source/ui/defines.hpp>
#include <source/ui/components/include/toast.hpp>
#include <source/utils/utils.hpp>
#include <source/vendor/json.hpp>
#include <windows.h>
#include <wininet.h>
#include <tlhelp32.h>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>
#pragma comment(lib, "wininet.lib")

typedef LONG(NTAPI* pfnNtSuspendProcess)(HANDLE);

static int s_launch_seq = 0;

static DWORD StartSuspended(const std::string& exePath)
{
    if (!std::filesystem::exists(exePath)) return 0;

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::string cmd = "\"" + exePath + "\"";
    if (!CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr,
        FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi))
        return 0;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
}

static void SuspendByPid(DWORD pid)
{
    if (!pid) return;
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return;
    pfnNtSuspendProcess NtSuspendProcess =
        (pfnNtSuspendProcess)GetProcAddress(hNtdll, "NtSuspendProcess");
    if (!NtSuspendProcess) return;

    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (hProcess) { NtSuspendProcess(hProcess); CloseHandle(hProcess); }
}

static std::string FindExeInBuild(const std::string& buildPath, const char* exeName)
{
    for (auto& entry : std::filesystem::recursive_directory_iterator(
        buildPath, std::filesystem::directory_options::skip_permission_denied))
    {
        if (entry.is_regular_file() && entry.path().filename() == exeName)
            return entry.path().string();
    }
    return "";
}

static DWORD StartArc(const std::string& buildPath, const std::string& gameArgs)
{
    std::string arcDir = buildPath + "\\Arc";
    std::string arcExe = arcDir + "\\Arc.exe";

    if (!std::filesystem::exists(arcExe))
        return 0;

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::string cmd = "\"" + arcExe + "\" " + gameArgs;
    if (!CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr,
        FALSE, 0, nullptr, arcDir.c_str(), &si, &pi))
        return 0;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
}

static DWORD FindProcessByName(const wchar_t* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe = {};
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;

    if (Process32First(snap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, name) == 0)
            {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &pe));
    }

    CloseHandle(snap);
    return pid;
}

static void StopArc()
{
    auto ArcPid = FindProcessByName(L"Arc.exe");
    if (ArcPid)
    {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, ArcPid);
        if (h)
        {
            TerminateProcess(h, 0);
            CloseHandle(h);
        }
        ArcPid = 0;
    }
}

static DWORD WaitForGameProcess(int timeoutSeconds)
{
    for (int i = 0; i < timeoutSeconds * 2; i++)
    {
        DWORD pid = FindProcessByName(L"Arc.exe");
        if (pid) return pid;
        Sleep(500);
    }
    return 0;
}

static bool IsProcessRunning(DWORD pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD exitCode = 0;
    GetExitCodeProcess(h, &exitCode);
    CloseHandle(h);
    return exitCode == STILL_ACTIVE;
}
static bool HasToDownload(const char* url, const std::string& destPath)
{
    HINTERNET hNet = InternetOpenA("RemixLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);

    if (!hNet)
        return true;

    bool hasTo = true;

    URL_COMPONENTSA uc = {};
    uc.dwStructSize = sizeof(uc);
    char host[256] = {};
    char path[1024] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = sizeof(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = sizeof(path);

    if (InternetCrackUrlA(url, 0, 0, &uc))
    {
        DWORD connectFlags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_FLAG_SECURE : 0;
        HINTERNET hConn = InternetConnectA(hNet, host, uc.nPort, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
        if (hConn)
        {
            DWORD reqFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | connectFlags;
            HINTERNET hReq = HttpOpenRequestA(hConn, "HEAD", path, nullptr, nullptr, nullptr, reqFlags, 0);
            if (hReq && HttpSendRequestA(hReq, nullptr, 0, nullptr, 0))
            {
                char clBuf[32] = {};
                DWORD clLen = sizeof(clBuf);
                ULONGLONG remoteSize = 0;
                if (HttpQueryInfoA(hReq, HTTP_QUERY_CONTENT_LENGTH, clBuf, &clLen, nullptr))
                    remoteSize = strtoull(clBuf, nullptr, 10);

                if (remoteSize > 0)
                {
                    WIN32_FILE_ATTRIBUTE_DATA fa = {};
                    if (GetFileAttributesExA(destPath.c_str(), GetFileExInfoStandard, &fa))
                    {
                        ULONGLONG localSize = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
                        hasTo = (localSize != remoteSize);
                    }
                }
            }
            if (hReq)
                InternetCloseHandle(hReq);
            InternetCloseHandle(hConn);
        }
    }

    InternetCloseHandle(hNet);
    return hasTo;
}

static bool DownloadFile(const char* url, const std::string& destPath, const char* displayName = nullptr)
{
    HINTERNET hNet = InternetOpenA("RemixLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hNet) return false;

    HINTERNET hUrl = InternetOpenUrlA(hNet, url, nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) { InternetCloseHandle(hNet); return false; }

    char clBuf[32] = {};
    DWORD clLen = sizeof(clBuf);
    ULONGLONG contentLength = 0;
    if (HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH, clBuf, &clLen, nullptr))
        contentLength = strtoull(clBuf, nullptr, 10);

    if (displayName) r_download_file_name = displayName;
    r_download_total_bytes = contentLength;
    r_download_bytes = 0;
    r_download_progress = 0.f;

    std::vector<unsigned char> bytes;
    unsigned char buf[65536];
    DWORD read = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0)
    {
        bytes.insert(bytes.end(), buf, buf + read);
        r_download_bytes = bytes.size();
        if (contentLength > 0)
            r_download_progress = (float)bytes.size() / (float)contentLength;
        read = 0;
    }
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);

    r_download_progress = 1.f;

    if (bytes.empty()) return false;

    HANDLE h = CreateFileA(destPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    WriteFile(h, bytes.data(), (DWORD)bytes.size(), &written, nullptr);
    CloseHandle(h);
    return written == (DWORD)bytes.size();
}

static bool SetupBuild(const std::string& buildPath)
{
    std::string arcDir  = buildPath + "\\Arc";
    std::string arcExe  = arcDir + "\\Arc.exe";
    std::string arcCfg  = arcDir + "\\Config.json";
    std::string remixExe = buildPath + "\\FortniteGame\\Binaries\\Win64\\RemixClient-Win64-Shipping.exe";

    std::filesystem::create_directories(arcDir);

    if (HasToDownload("https://r2.ploosh.dev/remix/Arc.exe", arcExe))
    {
        int totalFiles = 1 + (std::filesystem::exists(remixExe) ? 0 : 1);
        r_download_file_total = totalFiles;

        r_downloading = true;

        r_download_file_index = 1;
        DownloadFile("https://r2.ploosh.dev/remix/Arc.exe", arcExe, "Arc.exe");
        if (!std::filesystem::exists(arcExe))
        {
            r_downloading = false;
            return false;
        }
    }

    {
        nlohmann::json cfg;
        cfg["ClientID"] = "zszxfhpnjgzxzvpiiopxqrfpivjqhxbh";
        cfg["Executable"] = "FortniteGame\\Binaries\\Win64\\RemixClient-Win64-Shipping.exe";
        std::ofstream f(arcCfg);
        if (f) f << cfg.dump(4);
    }

    if (HasToDownload("https://r2.ploosh.dev/remix/RemixClient-Win64-Shipping.exe", remixExe))
    {
        if (!r_downloading)
        {
            r_download_file_total = 1;

            r_downloading = true;

            r_download_file_index = 1;
        }
        else 
            r_download_file_index = 2;
        std::filesystem::remove(buildPath + "\\FortniteGame\\Binaries\\Win64\\FortniteClient-Win64-Shipping-Remix.exe");
        std::filesystem::create_directories(buildPath + "\\FortniteGame\\Binaries\\Win64");
        if (!DownloadFile("https://r2.ploosh.dev/remix/RemixClient-Win64-Shipping.exe", remixExe, "RemixClient-Win64-Shipping.exe"))
        {
            r_downloading = false;
            return false;
        }
    }

    r_downloading = false;
    return true;
}

void c_launch_game::Launch(const std::string& buildPath)
{
    if (r_launch_state != LaunchState::Idle) return;
    r_launch_state = LaunchState::Launching;
    r_launch_error = "";

    s_launch_seq++;
    int seq = s_launch_seq;

    std::thread([buildPath, seq]() {
        DWORD existingArc = FindProcessByName(L"Arc.exe");
        if (existingArc)
        {
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, existingArc);
            if (h) { TerminateProcess(h, 0); CloseHandle(h); }
            Sleep(500);
        }

        r_launch_state = LaunchState::Launching;
        //toast.Push("Preparing files...", TOAST_INFO);

        if (!SetupBuild(buildPath))
        {
            //r_launch_error = "Failed to download required files.";
            r_launch_state = LaunchState::Idle;
            toast.Push("Failed to download required files.", TOAST_ERROR);
            return;
        }

        std::string arcDir = buildPath + "\\Arc";
        std::string arcCfg = arcDir + "\\Config.json";

        std::string arcClientID = "zszxfhpnjgzxzvpiiopxqrfpivjqhxbh";
        if (std::filesystem::exists(arcCfg))
        {
            try
            {
                std::ifstream cfgFile(arcCfg);
                auto cfgJson = nlohmann::json::parse(cfgFile);
                arcClientID = cfgJson.value("ClientID", arcClientID);
            }
            catch (...) {}
        }

        std::string arcToken;
        try
        {
            ArcConfiguration arcCfgObj;
            arcCfgObj.ClientID = arcClientID;
            arcCfgObj.stage = ArcStage::Dev;
            ArcInstance arc(arcCfgObj);

            std::string identity = arc.CreateIdentity(
                r_account_name.empty() ? "Remix" : r_account_name,
                r_account_id.empty() ? "0" : r_account_id,
                r_account_name.empty() ? "Player" : r_account_name
            );

            ArcSession session = arc.CreateAuthSession(identity);
            arcToken = session.auth.token;
        }
        catch (const std::exception& ex)
        {
            auto err = std::string("Failed to auth w/ Arc: ") + ex.what();
            //r_launch_error = std::string("Arc auth: ") + ex.what();
            r_launch_state = LaunchState::Idle;
            toast.Push(err.c_str(), TOAST_ERROR);
            return;
        }

        std::string launcherExe = FindExeInBuild(buildPath, "FortniteLauncher.exe");
        std::string eacExe = FindExeInBuild(buildPath, "FortniteClient-Win64-Shipping_EAC.exe");
        std::string eacEosExe = FindExeInBuild(buildPath, "FortniteClient-Win64-Shipping_EAC_EOS.exe");

        bool bEacEOS = eacExe.empty() && !eacEosExe.empty();

        if (s_launch_seq != seq) return;

        DWORD launcherPid = !launcherExe.empty() ? StartSuspended(launcherExe) : 0;
        DWORD eacPid = 0;
        if (!eacExe.empty()) eacPid = StartSuspended(eacExe);
        else if (!eacEosExe.empty()) eacPid = StartSuspended(eacEosExe);

        SuspendByPid(launcherPid);
        SuspendByPid(eacPid);

        std::string login    = r_account_name + "@project.remix";
        std::string password = r_auth_token.empty() ? "Rebooted" : r_auth_token;
        std::string fromfl   = bEacEOS ? "eaceos" : "eac";
        std::string caldera  = bEacEOS
            ? "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJhY2NvdW50X2lkIjoic2FyYWgiLCJnZW5lcmF0ZWQiOjE3NzQxMjEzNzksImNhbGRlcmFHdWlkIjoiZWYzMGE4MjUtMzFlNi00ZWRhLTlmM2EtZTI4YzgwNjYxODkxIiwiYWNQcm92aWRlciI6IkVhc3lBbnRpQ2hlYXRFT1MiLCJub3RlcyI6IiIsImZhbGxiYWNrIjpmYWxzZX0.f7EcDmsjj3XHzdbMl41YHyJE-LlAirjbjaqzcdqBB_Y"
            : "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJhY2NvdW50X2lkIjoiYmU5ZGE1YzJmYmVhNDQwN2IyZjQwZWJhYWQ4NTlhZDQiLCJnZW5lcmF0ZWQiOjE2Mzg3MTcyNzgsImNhbGRlcmFHdWlkIjoiMzgxMGI4NjMtMmE2NS00NDU3LTliNTgtNGRhYjNiNDgyYTg2IiwiYWNQcm92aWRlciI6IkVhc3lBbnRpQ2hlYXQiLCJub3RlcyI6IiIsImZhbGxiYWNrIjpmYWxzZX0.VAWQB67RTxhiWOxx7DBjnzDnXyyEnX7OljJm-j2d88G_WgwQ9wrE6lwMEHZHjBd1ISJdUO1UVUqkfLdU5nofBQ";

        std::string arcArgs =
            "-epicapp=Fortnite"
            " -epicenv=Prod"
            " -epiclocale=en-us"
            " -epicportal"
            " -skippatchcheck"
            " -nobe"
            " -nouac"
            " -nocodeguards"
            " -fromfl=" + fromfl +
            " -fltoken=3db3ba5dcbd2e16703f3978d"
            " -caldera=" + caldera +
            " -AUTH_LOGIN=" + login +
            " -AUTH_PASSWORD=" + password +
            " -AUTH_TYPE=epic"
            " -t=" + arcToken;

        DWORD arcPid = StartArc(buildPath, arcArgs);
        r_arc_pid = arcPid;

        if (!arcPid)
        {
            //r_launch_error = "Failed to start Arc.";
            r_launch_state = LaunchState::Idle;
            toast.Push("Failed to start Arc.", TOAST_ERROR);
            return;
        }

        r_launch_state = LaunchState::Running;

        DWORD gamePid = WaitForGameProcess(30);

        if (s_launch_seq != seq)
        {
            if (r_arc_pid == arcPid) StopArc();
            return;
        }

        if (!gamePid)
        {
            StopArc();
            //r_launch_error = "Game process not found.";
            r_launch_state = LaunchState::Idle;
            toast.Push("Game did not start.", TOAST_ERROR);
            return;
        }

        r_game_pid = gamePid;

        toast.Push("Game launched successfully!", TOAST_SUCCESS);

        while (r_launch_state == LaunchState::Running && FindProcessByName(L"Arc.exe"))
            Sleep(1000);

        if (s_launch_seq == seq)
        {
            r_game_pid = 0;
            StopArc();
            r_launch_state = LaunchState::Idle;
        }
    }).detach();
}

void c_launch_game::Stop()
{
    /*if (r_game_pid)
    {
        HANDLE hGame = OpenProcess(PROCESS_TERMINATE, FALSE, r_game_pid);
        if (hGame) { TerminateProcess(hGame, 0); CloseHandle(hGame); }
        r_game_pid = 0;
    }*/

    s_launch_seq++;
    StopArc();
    r_launch_state = LaunchState::Idle;
}
