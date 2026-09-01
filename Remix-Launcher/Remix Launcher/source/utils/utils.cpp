#include <source/utils/utils.hpp>
#include <source/includes.hpp>
#include <shlobj.h>
#include <fstream>
#pragma comment(lib, "Version.lib")

static ID3D11ShaderResourceView* LoadBMPTexture(const std::string& file_path, int* out_width, int* out_height)
{
    HBITMAP bitmap = (HBITMAP)LoadImageA(nullptr, file_path.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!bitmap) return nullptr;

    BITMAP bm_info = {};
    GetObject(bitmap, sizeof(bm_info), &bm_info);

    int width = bm_info.bmWidth;
    int height = abs(bm_info.bmHeight);

    if (width == 0 || height == 0)
    {
        DeleteObject(bitmap);
        return nullptr;
    }

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = width;
    bi.biHeight = -height;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    std::vector<BYTE> pixels(width * height * 4);
    HDC screen_dc = GetDC(nullptr);
    GetDIBits(screen_dc, bitmap, 0, height, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, screen_dc);
    DeleteObject(bitmap);

    for (int i = 3; i < width * height * 4; i += 4)
        pixels[i] = 255;

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = width;
    tex_desc.Height = height;
    tex_desc.MipLevels = 0;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* texture = nullptr;
    if (FAILED(r_device->CreateTexture2D(&tex_desc, nullptr, &texture)))
        return nullptr;

    r_device_context->UpdateSubresource(texture, 0, nullptr, pixels.data(), width * 4, 0);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = tex_desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = -1;

    ID3D11ShaderResourceView* srv = nullptr;
    r_device->CreateShaderResourceView(texture, &srv_desc, &srv);
    r_device_context->GenerateMips(srv);
    texture->Release();

    *out_width = width;
    *out_height = height;
    return srv;
}

ID3D11ShaderResourceView* c_utils::LoadImageFromMemory(const unsigned char* data, size_t size, int* out_width, int* out_height)
{
    IWICImagingFactory* wic = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
        return nullptr;

    IWICStream* stream = nullptr;
    wic->CreateStream(&stream);
    stream->InitializeFromMemory((BYTE*)data, (DWORD)size);

    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
    {
        stream->Release();
        wic->Release();
        return nullptr;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);

    IWICFormatConverter* converter = nullptr;
    wic->CreateFormatConverter(&converter);
    converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);

    UINT width, height;
    converter->GetSize(&width, &height);

    std::vector<BYTE> pixels(width * height * 4);
    converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());

    converter->Release();
    frame->Release();
    decoder->Release();
    stream->Release();
    wic->Release();

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = width;
    tex_desc.Height = height;
    tex_desc.MipLevels = 0;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* texture = nullptr;
    if (FAILED(r_device->CreateTexture2D(&tex_desc, nullptr, &texture)))
        return nullptr;

    r_device_context->UpdateSubresource(texture, 0, nullptr, pixels.data(), width * 4, 0);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = tex_desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = -1;

    ID3D11ShaderResourceView* srv = nullptr;
    r_device->CreateShaderResourceView(texture, &srv_desc, &srv);
    r_device_context->GenerateMips(srv);
    texture->Release();

    *out_width = (int)width;
    *out_height = (int)height;
    return srv;
}

std::string c_utils::GetBuildVersion(const std::string& path)
{
    const char* candidates[] = {
        "\\Engine\\Binaries\\Win64\\CrashReportClient.exe",
    };

    for (auto& rel : candidates)
    {
        std::wstring full(path.begin(), path.end());
        full += std::wstring(rel, rel + strlen(rel));

        DWORD dummy = 0;
        DWORD size = GetFileVersionInfoSizeW(full.c_str(), &dummy);
        if (!size) continue;

        std::vector<BYTE> buf(size);
        if (!GetFileVersionInfoW(full.c_str(), 0, size, buf.data())) continue;

        wchar_t* pVal = nullptr;
        UINT len = 0;
        if (!VerQueryValueW(buf.data(), L"\\StringFileInfo\\040904B0\\ProductVersion", (void**)&pVal, &len)) continue;
        if (!pVal || !len) continue;

        int needed = WideCharToMultiByte(CP_UTF8, 0, pVal, -1, nullptr, 0, nullptr, nullptr);
        std::string raw(needed > 0 ? needed - 1 : 0, '\0');
        if (needed > 0) WideCharToMultiByte(CP_UTF8, 0, pVal, -1, raw.data(), needed, nullptr, nullptr);
        return raw;
    }

    return "";
}

bool c_utils::GetScreenSize()
{
    r_width = (float)GetSystemMetrics(SM_CXSCREEN);
    r_height = (float)GetSystemMetrics(SM_CYSCREEN);

	if (r_width != 0 && r_height != 0)
		return true;

	return false;
}

void c_utils::AddBuild(const std::string& path)
{
    if (path.empty()) return;

    const char* required[] = {
        "\\FortniteGame\\Binaries\\Win64",
        "\\FortniteGame\\Content",
        "\\Engine\\Binaries\\Win64",
    };

    for (auto& rel : required)
    {
        std::string check = path + rel;
        DWORD attr = GetFileAttributesA(check.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            toast.Push("Invalid build folder.", TOAST_ERROR);
            return;
        }
    }

    for (auto& b : r_builds)
        if (b.path == path) { toast.Push("Build already imported.", TOAST_ERROR); return; }

    Build build;
    build.path = path;
    size_t slash_pos = path.rfind('\\');
    build.name = slash_pos != std::string::npos ? path.substr(slash_pos + 1) : path;

    std::string splash_path = path + "\\FortniteGame\\Content\\Splash\\Splash.bmp";
    build.splash_texture = LoadBMPTexture(splash_path, &build.splash_width, &build.splash_height);
    build.version = GetBuildVersion(path);

    r_builds.push_back(build);
    SaveBuilds();
}

void c_utils::EnsureAssetFile(const std::wstring& path, const unsigned char* data, size_t size)
{
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(h, data, (DWORD)size, &written, nullptr);
    CloseHandle(h);
}

std::wstring c_utils::GetRemixDir()
{
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return L"";
    std::wstring path = std::wstring(local) + L"\\Remix Launcher\\";
    CoTaskMemFree(local);
    CreateDirectoryW(path.c_str(), nullptr);
    return path;
}

std::wstring c_utils::GetAssetsDir()
{
    std::wstring dir = c_utils::GetRemixDir() + L"assets\\";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

ID3D11ShaderResourceView* c_utils::LoadImageFromFile(const wchar_t* path, int* out_width, int* out_height)
{
    IWICImagingFactory* wic = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
        return nullptr;

    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(wic->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)))
    {
        wic->Release();
        return nullptr;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);

    IWICFormatConverter* converter = nullptr;
    wic->CreateFormatConverter(&converter);
    converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);

    UINT width, height;
    converter->GetSize(&width, &height);

    std::vector<BYTE> pixels(width * height * 4);
    converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());

    converter->Release();
    frame->Release();
    decoder->Release();
    wic->Release();

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = width;
    tex_desc.Height = height;
    tex_desc.MipLevels = 0;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* texture = nullptr;
    if (FAILED(r_device->CreateTexture2D(&tex_desc, nullptr, &texture)))
        return nullptr;

    r_device_context->UpdateSubresource(texture, 0, nullptr, pixels.data(), width * 4, 0);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = tex_desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = -1;

    ID3D11ShaderResourceView* srv = nullptr;
    r_device->CreateShaderResourceView(texture, &srv_desc, &srv);
    r_device_context->GenerateMips(srv);
    texture->Release();

    *out_width = (int)width;
    *out_height = (int)height;
    return srv;
}

static std::string GetBuildsFilePath()
{
    std::wstring wide = c_utils::GetRemixDir() + L"builds.txt";
    return std::string(wide.begin(), wide.end());
}

void c_utils::SaveBuilds()
{
    std::ofstream file(GetBuildsFilePath());
    for (auto& build : r_builds)
        file << build.path << "\n";
}

void c_utils::LoadBuilds()
{
    std::ifstream file(GetBuildsFilePath());
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty())
            AddBuild(line);
    }
}

bool c_utils::GetStartupEnabled()
{
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;

    DWORD type, size = 0;
    bool exists = RegQueryValueExW(key, L"RemixLauncher", nullptr, &type, nullptr, &size) == ERROR_SUCCESS;
    RegCloseKey(key);
    return exists;
}

void c_utils::SetStartupEnabled(bool enabled)
{
    HKEY key;
    RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &key);

    if (enabled)
    {
        wchar_t exe_path[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        RegSetValueExW(key, L"RemixLauncher", 0, REG_SZ, (BYTE*)exe_path, (DWORD)((wcslen(exe_path) + 1) * sizeof(wchar_t)));
    }
    else
    {
        RegDeleteValueW(key, L"RemixLauncher");
    }

    RegCloseKey(key);
}

std::string c_utils::BrowseFolder()
{
    std::string result;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        return result;

    DWORD options;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);

    if (SUCCEEDED(dialog->Show(r_window)))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)))
        {
            PWSTR folder_path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &folder_path)))
            {
                std::wstring wide(folder_path);
                result = std::string(wide.begin(), wide.end());
                CoTaskMemFree(folder_path);
            }
            item->Release();
        }
    }

    dialog->Release();
    return result;
}
