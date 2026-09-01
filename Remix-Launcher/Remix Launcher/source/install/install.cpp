#include <source/install/install.hpp>
#include <source/vendor/json.hpp>
#include <windows.h>
#include <shlobj.h>
#include <wininet.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <thread>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "bcrypt.lib")

#define BACKEND_BASE_INST "https://remixogfn.dev"

static std::wstring GetCurrentExePath()
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return buf;
}

static std::wstring GetExeFileName()
{
    std::wstring p = GetCurrentExePath();
    auto pos = p.rfind(L'\\');
    return pos != std::wstring::npos ? p.substr(pos + 1) : p;
}

static std::wstring GetInstallDir()
{
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return L"";
    std::wstring dir = std::wstring(local) + L"\\Remix Launcher";
    CoTaskMemFree(local);
    return dir;
}

static std::wstring GetInstallPath()
{
    return GetInstallDir() + L"\\" + GetExeFileName();
}

static bool PathsEqual(const std::wstring& a, const std::wstring& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (towlower(a[i]) != towlower(b[i])) return false;
    return true;
}

static void CreateShortcut(const std::wstring& lnkPath, const std::wstring& targetPath)
{
    IShellLinkW* psl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, (void**)&psl))) return;

    psl->SetPath(targetPath.c_str());
    psl->SetIconLocation(targetPath.c_str(), 0);
    std::wstring workDir = targetPath.substr(0, targetPath.rfind(L'\\'));
    psl->SetWorkingDirectory(workDir.c_str());

    IPersistFile* ppf = nullptr;
    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf)))
    {
        ppf->Save(lnkPath.c_str(), TRUE);
        ppf->Release();
    }
    psl->Release();
}

static std::string SHA256File(const std::wstring& path)
{
    HANDLE hf = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return "";

    BCRYPT_ALG_HANDLE  hAlg  = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string result;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0)
    {
        DWORD hashLen = 0, cbData = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
            (PUCHAR)&hashLen, sizeof(hashLen), &cbData, 0);

        if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0)
        {
            unsigned char buf[65536]; DWORD r = 0;
            while (ReadFile(hf, buf, sizeof(buf), &r, nullptr) && r > 0)
                BCryptHashData(hHash, buf, r, 0);

            std::vector<unsigned char> hash(hashLen);
            BCryptFinishHash(hHash, hash.data(), hashLen, 0);
            BCryptDestroyHash(hHash);

            const char* hex = "0123456789abcdef";
            for (auto b : hash) { result += hex[b >> 4]; result += hex[b & 0xf]; }
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    CloseHandle(hf);
    return result;
}

static std::string InstHttpGet(const char* url)
{
    std::string result;
    HINTERNET hi = InternetOpenA("RemixLauncher/1.0", INTERNET_OPEN_TYPE_DIRECT,
        nullptr, nullptr, 0);
    if (!hi) return result;
    HINTERNET hu = InternetOpenUrlA(hi, url, nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (hu)
    {
        char buf[4096]; DWORD r = 0;
        while (InternetReadFile(hu, buf, sizeof(buf) - 1, &r) && r > 0)
        { buf[r] = '\0'; result += buf; r = 0; }
        InternetCloseHandle(hu);
    }
    InternetCloseHandle(hi);
    return result;
}

static bool InstHttpDownload(const char* url, const std::wstring& dest)
{
    HINTERNET hi = InternetOpenA("RemixLauncher/1.0", INTERNET_OPEN_TYPE_DIRECT,
        nullptr, nullptr, 0);
    if (!hi) return false;
    HINTERNET hu = InternetOpenUrlA(hi, url, nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (!hu) { InternetCloseHandle(hi); return false; }

    HANDLE hf = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE)
    {
        InternetCloseHandle(hu);
        InternetCloseHandle(hi);
        return false;
    }

    unsigned char buf[8192]; DWORD r = 0;
    bool ok = true;
    while (InternetReadFile(hu, buf, sizeof(buf), &r) && r > 0)
    {
        DWORD w;
        if (!WriteFile(hf, buf, r, &w, nullptr)) { ok = false; break; }
        r = 0;
    }

    CloseHandle(hf);
    InternetCloseHandle(hu);
    InternetCloseHandle(hi);
    if (!ok) DeleteFileW(dest.c_str());
    return ok;
}

void c_installer::CheckAndInstall()
{
    std::wstring installPath = GetInstallPath();
    std::wstring currentPath = GetCurrentExePath();

    DeleteFileW((installPath + L".old").c_str());

    if (PathsEqual(currentPath, installPath)) return;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    CreateDirectoryW(GetInstallDir().c_str(), nullptr);

    if (!CopyFileW(currentPath.c_str(), installPath.c_str(), FALSE))
    {
        CoUninitialize();
        return;
    }

    PWSTR desktop = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop)))
    {
        CreateShortcut(std::wstring(desktop) + L"\\Remix Launcher.lnk", installPath);
        CoTaskMemFree(desktop);
    }

    PWSTR programs = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &programs)))
    {
        std::wstring dir = std::wstring(programs) + L"\\Remix Launcher";
        CoTaskMemFree(programs);
        CreateDirectoryW(dir.c_str(), nullptr);
        CreateShortcut(dir + L"\\Remix Launcher.lnk", installPath);
    }

    CoUninitialize();

    ShellExecuteW(nullptr, L"open", installPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    ExitProcess(0);
}

void c_installer::CheckForUpdate()
{
    std::thread([]() {
        std::string resp = InstHttpGet(BACKEND_BASE_INST "/assets/launcher/version.json");
        if (resp.empty()) return;

        try
        {
            auto j = nlohmann::json::parse(resp);
            std::string remoteHash = j.value("signature", "");
            std::string url = j.value("url", "");
            if (remoteHash.empty() || url.empty()) return;

            std::wstring exePath = GetCurrentExePath();
            if (SHA256File(exePath) == remoteHash)
                return;

            std::wstring newPath = exePath + L".new";
            if (!InstHttpDownload(url.c_str(), newPath)) return;

            std::wstring oldPath = exePath + L".old";
            DeleteFileW(oldPath.c_str());
            MoveFileW(exePath.c_str(), oldPath.c_str());
            MoveFileExW(newPath.c_str(), exePath.c_str(), MOVEFILE_REPLACE_EXISTING);

            ShellExecuteW(nullptr, L"open", exePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            ExitProcess(0);
        }
        catch (...) {}
    }).detach();
}
