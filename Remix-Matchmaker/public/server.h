#include <string>

struct MMServer
{
    std::string ip;
    uint16_t port = 0;
    std::string sessionId;
    std::string region;
    std::string playlist;
    bool starting = true;
    int queuedPlayers = 0;
    int maxPlayers = 0;

    bool operator==(const MMServer& _Other) const
    {
        return sessionId == _Other.sessionId;
    }
};
void to_json(nlohmann::json& nlohmann_json_j, const MMServer& nlohmann_json_t);
void from_json(const nlohmann::json& nlohmann_json_j, MMServer& nlohmann_json_t);