#pragma once
#include <string>

class c_api
{
public:
    static void FetchPlayerCount();
    static void FetchProfile();
    static void FetchFriends();
    static void AddFriend(const std::string& friendAccountId);
    static void RemoveFriend(const std::string& friendAccountId);
    static void SearchPlayers(const char* query);
    static void LoginWithDiscord();
    static void SaveSession();
    static void ClearSession();
    static void RestoreSession();
    static void LoginWithArc(const std::string& iss, const std::string& sub, const std::string& dn);
};

inline c_api api;
