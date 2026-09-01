#pragma once
#include <string>
#include <d3d11.h>

class c_utils
{
public:
    static bool GetScreenSize();
    static void AddBuild(const std::string& path);
    static std::string BrowseFolder();
    static ID3D11ShaderResourceView* LoadImageFromMemory(const unsigned char* data, size_t size, int* out_width, int* out_height);
    static ID3D11ShaderResourceView* LoadImageFromFile(const wchar_t* path, int* out_width, int* out_height);
    static std::wstring GetRemixDir();
    static std::wstring GetAssetsDir();  
    static void EnsureAssetFile(const std::wstring& path, const unsigned char* data, size_t size);
    static void SaveBuilds();
    static void LoadBuilds();
    static bool GetStartupEnabled();
    static void SetStartupEnabled(bool enabled);
    static std::string GetBuildVersion(const std::string& path);
};

inline c_utils utils;
