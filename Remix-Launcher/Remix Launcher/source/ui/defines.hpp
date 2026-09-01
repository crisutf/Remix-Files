#pragma once
#include <source/includes.hpp>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <source/launch/launch_game.hpp>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

inline ID3D11Device* r_device = nullptr;
inline ID3D11DeviceContext* r_device_context = nullptr;
inline IDXGISwapChain* r_swap_chain = nullptr;
inline ID3D11RenderTargetView* r_render_target_view = nullptr;

inline UINT r_resize_width = 0;
inline UINT r_resize_height = 0;

inline bool r_occluded = false;
inline HWND r_window = nullptr;

inline float r_width;
inline float r_height;

inline ImFont* r_font_main;
inline ImFont* r_font_icon;

inline bool r_auth_done = false;
inline int r_page = 0;
inline std::string r_auth_token = "";
inline std::string r_discord_state = "";
inline bool r_discord_polling = false;

inline float r_sidebar_width = 220.f;
inline float r_titlebar_height = 40.f;
inline float r_dpi_scale = 1.f;

inline std::string r_account_name = "";
inline std::string r_account_id = "";
inline std::string r_profile_picture_url = "";

struct Build
{
    std::string name, path, version;
    ID3D11ShaderResourceView* splash_texture = nullptr;
    int splash_width = 0;
    int splash_height = 0;
};

inline std::vector<Build> r_builds;

struct SearchResult
{
    std::string name;
    std::string accountId;
    std::string profilePictureUrl;
};

struct FriendEntry
{
    std::string accountId;
    std::string displayName;
    std::string profilePictureUrl;
    bool online = false;
    std::string status;
};

struct FriendAvatar
{
    ID3D11ShaderResourceView* texture = nullptr;
    int width = 0;
    int height = 0;
    std::string url;
    std::vector<unsigned char> pendingBytes;
    bool pending = false;
    bool loadRequested = false;
};

inline std::vector<SearchResult> r_search_results;
inline bool r_searching = false;
inline std::vector<FriendEntry> r_friends;
inline std::unordered_set<std::string> r_friend_ids;
inline std::unordered_set<std::string> r_pending_friend_ids;
inline std::unordered_map<std::string, FriendAvatar> r_friend_avatars;
inline std::mutex r_friend_avatars_mtx;

inline float r_launcher_width = 1280.f;
inline float r_launcher_height = 780.f;

inline float sc(float v) { return v * r_dpi_scale; }

inline bool r_dpi_changed = false;

inline std::vector<unsigned char> r_pending_profile_bytes;
inline bool r_profile_picture_pending = false;

inline ID3D11ShaderResourceView* r_banner_texture = nullptr;
inline int r_banner_width = 0;
inline int r_banner_height = 0;

struct BannerEntry
{
    ID3D11ShaderResourceView* texture = nullptr;
    int width = 0;
    int height = 0;
};
inline std::vector<BannerEntry> r_banners;

inline ID3D11ShaderResourceView* r_logo_texture = nullptr;
inline int r_logo_width = 0;
inline int r_logo_height = 0;

inline ID3D11ShaderResourceView* r_profile_texture = nullptr;
inline int r_profile_width = 0;
inline int r_profile_height = 0;

inline int r_player_count = 0;
inline int r_server_status = 0;

inline LaunchState r_launch_state = LaunchState::Idle;
inline std::string r_launch_error  = "";
inline DWORD r_game_pid = 0;
inline DWORD r_arc_pid = 0;

inline bool r_downloading = false;
inline std::string r_download_file_name = "";
inline int r_download_file_index = 0;
inline int r_download_file_total = 0;
inline float r_download_progress = 0.f;
inline size_t r_download_bytes = 0;
inline size_t r_download_total_bytes = 0;

inline bool r_pending_add_build = false;
inline std::string r_pending_add_build_path = "";

inline bool r_logout_confirm = false;

inline std::string r_account_role = "";
inline ImU32 r_account_role_color = IM_COL32(90, 90, 112, 255);

inline ArcInstance* r_arc_instance = nullptr;