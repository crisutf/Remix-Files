#include "../public/utils.h"
#include "../public/global.h"
#include "../public/gs_socket.h"

std::unordered_map<seasocks::WebSocket*, std::shared_ptr<MMServer>> GSServerMap;

void GSHandler::onConnect(seasocks::WebSocket* socket)
{
    printf("[GS] Got a connection!\n");
}

void GSHandler::onData(seasocks::WebSocket* socket, const char* data)
{
    nlohmann::json jdata = nlohmann::json::parse(data);

    if (!jdata.contains("type") || !jdata.contains("payload"))
        return;

    std::string type = jdata["type"];
    nlohmann::json payload = jdata["payload"];

    printf("[GS] Message type: %s\n", type.c_str());
    if (type == "create")
    {
        std::string playlist = payload["playlist"];
        std::transform(playlist.begin(), playlist.end(), playlist.begin(), ::tolower);

        std::lock_guard<std::mutex> lk(gMtx);
        auto srv = std::make_shared<MMServer>();

        srv->ip = payload["ip"];
        srv->port = payload["port"];
        srv->sessionId = payload["id"];
        srv->region = payload["region"];
        srv->playlist = playlist;
        srv->maxPlayers = payload["maxPlayers"];

        printf("Adding server:\n- Session: %s\n- Playlist: %s\n- Region: %s\n- IP: %s\n- Port: %d\n", srv->sessionId.c_str(), playlist.c_str(), srv->region.c_str(),
            srv->ip.c_str(), srv->port);

        GSServerMap[socket] = srv;
        ServerMap[payload["region"]][playlist].push_back(srv);
        ServerMapById[srv->sessionId] = srv;
    }
    else if (type == "ready")
    {
        std::lock_guard<std::mutex> lk(gMtx);
        auto& server = GSServerMap[socket];

        if (server)
            server->starting = false;
    }
}

void GSHandler::onDisconnect(seasocks::WebSocket* socket)
{
    printf("[GS] Closed\n");
    std::lock_guard<std::mutex> lk(gMtx);
    auto& server = GSServerMap[socket];

    if (server)
    {
        auto& serverList = ServerMap[server->region][server->playlist];

        serverList.erase(find(serverList.begin(), serverList.end(), server));
        ServerMapById.erase(server->sessionId);
        GSServerMap.erase(socket);
    }
}