#pragma once
#include <string>
#include <vector>
#include <optional>

enum class ArcStage
{
    Dev,
    Staging,
    Prod
};

struct ArcDomain
{
    std::string service;
    std::string host;
    bool ssl;
    ArcStage type;
};

struct ArcConfiguration
{
    std::string Authorization;
    std::string ClientID;
    ArcStage stage = ArcStage::Prod;
};

struct ArcAccount
{
    std::string display_name;
    std::string country;
    bool active = false;
};

struct ArcAuthInfo
{
    std::string clid;
    std::string token;
};

struct ArcSession
{
    ArcAccount account;
    ArcAuthInfo auth;
};

class ArcInstance
{
public:
    const std::string ClientID;
    const ArcStage stage;
    const std::vector<ArcDomain> Hosts;

    ArcInstance(const ArcConfiguration& config);

    std::optional<ArcDomain> GetAuthDomain() const;
    std::optional<ArcDomain> GetAccountDomain() const;

    std::string CreateIdentity(const std::string& iss, const std::string& sub, const std::string& dn) const;
    ArcSession CreateAuthSession(const std::string& identity) const;
};

namespace ArcUtil
{
    std::string Base64Url(const std::string& json);
    std::string JWT(const std::string& payload_json);
}
