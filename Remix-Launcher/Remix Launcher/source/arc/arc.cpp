#include <source/arc/arc.hpp>
#include <source/vendor/json.hpp>
#include <windows.h>
#include <winhttp.h>
#include <chrono>
#include <algorithm>
#include <stdexcept>

#pragma comment(lib, "winhttp.lib")

static const std::vector<ArcDomain> AllDomains =
{
    { "auth",    "dev-auth-v1.arc-services.dev",        true, ArcStage::Dev },
    { "account", "dev-account-v1.arc-services.dev",     true, ArcStage::Dev },
    { "auth",    "staging-auth-v1.arc-services.dev",    true, ArcStage::Staging },
    { "account", "staging-account-v1.arc-services.dev", true, ArcStage::Staging },
    { "auth",    "prod-auth-v1.arc-services.dev",       true, ArcStage::Prod },
    { "account", "prod-account-v1.arc-services.dev",    true, ArcStage::Prod },
};

static const char kBase64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string ArcUtil::Base64Url(const std::string& input)
{
    std::string out;
    int val = 0;
    int bits = -6;
    for (unsigned char c : input)
    {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0)
        {
            out.push_back(kBase64Table[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6)
        out.push_back(kBase64Table[(val << (-bits)) & 0x3F]);

    for (auto& ch : out)
    {
        if (ch == '+') ch = '-';
        else if (ch == '/') ch = '_';
    }
    while (!out.empty() && out.back() == '=')
        out.pop_back();

    return out;
}

std::string ArcUtil::JWT(const std::string& payload_json)
{
    nlohmann::json header;
    header["alg"] = "none";
    header["typ"] = "JWT";

    return Base64Url(header.dump()) + "." + Base64Url(payload_json) + ".";
}

ArcInstance::ArcInstance(const ArcConfiguration& config)
    : ClientID(config.ClientID),
      stage(config.stage),
      Hosts([&]()
      {
          std::vector<ArcDomain> filtered;
          for (auto& d : AllDomains)
          {
              if (d.type == config.stage)
                  filtered.push_back(d);
          }
          return filtered;
      }())
{
}

std::optional<ArcDomain> ArcInstance::GetAuthDomain() const
{
    for (auto& d : Hosts)
    {
        if (d.service == "auth")
            return d;
    }
    return std::nullopt;
}

std::optional<ArcDomain> ArcInstance::GetAccountDomain() const
{
    for (auto& d : Hosts)
    {
        if (d.service == "account")
            return d;
    }
    return std::nullopt;
}

std::string ArcInstance::CreateIdentity(const std::string& iss, const std::string& sub, const std::string& dn) const
{
    auto now = std::chrono::system_clock::now();
    long long iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    std::string clean_dn;
    for (char c : dn)
    {
        if (static_cast<unsigned char>(c) <= 0xFF)
            clean_dn.push_back(c);
    }

    nlohmann::json payload;
    payload["iat"] = iat;
    payload["exp"] = iat + 300;
    payload["iss"] = iss;
    payload["sub"] = sub;
    payload["dn"]  = clean_dn;

    return ArcUtil::JWT(payload.dump());
}

static std::string ArcHttpPost(const std::string& url, const std::string& body,
                               const std::string& client_id)
{
    std::string result;

    std::wstring wurl(url.begin(), url.end());

    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t path[1024] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
        throw std::runtime_error("failed to parse url");

    bool is_https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(L"ArcClient/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        throw std::runtime_error("failed to open session");

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("failed to connect");
    }

    DWORD reqFlags = is_https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path, nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, reqFlags);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("failed to open request");
    }

    if (is_https)
    {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    std::string headerStr = "Content-Type: application/json\r\nX-Arc-Client: " + client_id + "\r\n";
    std::wstring wHeaders(headerStr.begin(), headerStr.end());

    BOOL sent = WinHttpSendRequest(hRequest, wHeaders.c_str(), (DWORD)wHeaders.length(), (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);

    if (!sent)
    {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("failed to send request, error: " + std::to_string(err));
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr))
    {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("failed to receive response, error: " + std::to_string(err));
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
    {
        std::vector<char> buf(bytesAvailable + 1);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead))
        {
            buf[bytesRead] = '\0';
            result.append(buf.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (statusCode < 200 || statusCode >= 300)
        throw std::runtime_error("failed to create auth session: " + std::to_string(statusCode));

    return result;
}

ArcSession ArcInstance::CreateAuthSession(const std::string& identity) const
{
    auto domain = GetAuthDomain();
    if (!domain.has_value())
        throw std::runtime_error("auth domain not found for the specified stage");

    std::string protocol = domain->ssl ? "https" : "http";
    std::string url = protocol + "://" + domain->host + "/api/v1/auth/session";

    nlohmann::json req_body;
    req_body["identity"] = identity;

    std::string response = ArcHttpPost(url, req_body.dump(), ClientID);

    nlohmann::json j;
    try
    {
        j = nlohmann::json::parse(response);
    }
    catch (...)
    {
        throw std::runtime_error("error parsing response json");
    }

    ArcSession session;
    if (j.contains("account"))
    {
        auto& acc = j["account"];
        session.account.display_name = acc.value("display_name", "");
        session.account.country = acc.value("country", "");
        session.account.active = acc.value("active", false);
    }
    if (j.contains("auth"))
    {
        auto& auth = j["auth"];
        session.auth.clid = auth.value("clid", "");
        session.auth.token = auth.value("token", "");
    }

    return session;
}
