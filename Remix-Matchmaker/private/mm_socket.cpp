#include "../public/utils.h"
#include "../public/global.h"
#include "../public/sha256.h"
#include "../public/mm_socket.h"

std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<seasocks::WebSocket*>>> QueueMap;
std::unordered_map<seasocks::WebSocket*, std::pair<std::string, std::string>> SocketMap;
std::unordered_map<seasocks::WebSocket*, std::string> TicketMap;
std::unordered_map<seasocks::WebSocket*, std::thread::native_handle_type> ThreadMap;

void MMHandler::dropSocket(seasocks::WebSocket* socket)
{
    std::lock_guard<std::mutex> lk(gMtx);

    auto th = ThreadMap.find(socket);
    if (th != ThreadMap.end())
    {
        pthread_cancel(th->second);
        ThreadMap.erase(th);
    }
    TicketMap.erase(socket);
    connections.erase(socket);
    auto sm = SocketMap.find(socket);
    if (sm != SocketMap.end())
    {
        QueueMap[sm->second.first][sm->second.second].erase(socket);
        SocketMap.erase(sm);
    }
}
void MMHandler::closeSocket(seasocks::WebSocket* socket)
{
    _server->execute(
        [=]()
        {
            socket->close();
        });
    dropSocket(socket);
}
void MMHandler::onConnect(seasocks::WebSocket* socket)
{
    printf("Got a connection!\n");
    auto _Thread = std::thread(
        [=, this]()
        {
            {
                std::lock_guard<std::mutex> lk(gMtx);
                connections.insert(socket);
            }
            auto _authHeader = socket->getHeader("Authorization");

            printf("Auth header: %s\n", _authHeader.c_str());
            auto _authHeaderPrts = split(_authHeader, " ");
            auto signature = b64DecodeBinary(_authHeaderPrts[3]);
            if (signature.size() != 32)
            {
                printf("Invalid signature size: disconnecting.");
                return closeSocket(socket);
            }
            auto& _payloadB64 = _authHeaderPrts[2];
            auto k = "ed14ba700b1aeb4103b457c2a43028e0";
            SHA256_HMAC sh(k, strlen(k));
            sh.update(_payloadB64.c_str());
            std::vector<uint8_t> r2 = sh.finalize();
            if (memcmp(r2.data(), signature.data(), 32) != 0)
            {
                printf("Invalid signature: disconnecting.");
                return closeSocket(socket);
            }
            auto _payload = b64Decode(_payloadB64);
            nlohmann::json payload = nlohmann::json::parse(_payload);

            const auto p1 = std::chrono::system_clock::now();

            if (payload["exp"] < std::chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count())
            {
                printf("Data has expired: disconnecting.");
                return closeSocket(socket);
            }

            auto& region = payload["region"];
            std::string playlist = payload["playlist"];

            std::transform(playlist.begin(), playlist.end(), playlist.begin(), ::tolower);

            printf("Player %s matchmaking into bucket %s\n", std::string(payload["accountId"]).c_str(), std::string(payload["bucketId"]).c_str());

            _server->execute(
                [=]()
                {
                    nlohmann::json data, update_payload;
                    data["name"] = "StatusUpdate";
                    update_payload["state"] = "Connecting";
                    data["payload"] = update_payload;
                    socket->send(data.dump());
                });

            _server->execute(
                [=, this]()
                {
                    nlohmann::json data, update_payload;
                    data["name"] = "StatusUpdate";
                    update_payload["state"] = "Waiting";
                    update_payload["totalPlayers"] = connections.size();
                    update_payload["connectedPlayers"] = 0;
                    data["payload"] = update_payload;
                    socket->send(data.dump());
                });

            {
                std::string ticketId = randUUID();
                {
                    std::lock_guard<std::mutex> lk(gMtx);
                    QueueMap[region][playlist].insert(socket);
                    SocketMap[socket] = std::pair<std::string, std::string>(region, playlist);
                    TicketMap[socket] = ticketId;
                }

                _server->execute(
                    [=]()
                    {
                        std::lock_guard<std::mutex> lk(gMtx);
                        nlohmann::json data, update_payload;

                        data["name"] = "StatusUpdate";
                        update_payload["state"] = "Queued";
                        update_payload["ticketId"] = ticketId;
                        update_payload["queuedPlayers"] = QueueMap[region][playlist].size();
                        update_payload["estimatedWaitSec"] = 60 + (rand() % 15);
                        update_payload["status"] = nlohmann::json();
                        data["payload"] = update_payload;
                        socket->send(data.dump());
                    });
            }

            while (true)
            {
                {
                    std::lock_guard<std::mutex> lk(gMtx);
                    if (SocketMap.find(socket) == SocketMap.end())
                        return;
                }

                std::shared_ptr<MMServer> pickedServer;
                {
                    std::lock_guard<std::mutex> lk(gMtx);
                    for (auto server : ServerMap[region][playlist])
                    {
                        if (server->queuedPlayers < server->maxPlayers)
                        {
                            server->queuedPlayers++;
                            pickedServer = server;
                            break;
                        }
                    }
                }

                if (!pickedServer)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                {
                    std::lock_guard<std::mutex> lk(gMtx);
                    QueueMap[region][playlist].erase(socket);
                }

                printf("Joining server %s\n", pickedServer->sessionId.c_str());
                auto server = pickedServer;
                auto matchId = randUUID();

                _server->execute(
                    [=]()
                    {
                        nlohmann::json data, update_payload;
                        data["name"] = "StatusUpdate";
                        update_payload["state"] = "SessionAssignment";
                        update_payload["matchId"] = matchId;
                        data["payload"] = update_payload;
                        socket->send(data.dump());
                    });

                while (server->starting)
                {
                    {
                        std::lock_guard<std::mutex> lk(gMtx);
                        if (SocketMap.find(socket) == SocketMap.end())
                            return;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                _server->execute(
                    [=]()
                    {
                        nlohmann::json data, update_payload;
                        data["name"] = "Play";
                        update_payload["matchId"] = matchId;
                        update_payload["sessionId"] = server->sessionId;
                        update_payload["joinDelaySec"] = 0;
                        data["payload"] = update_payload;
                        socket->send(data.dump());
                    });

                {
                    std::lock_guard<std::mutex> lk(gMtx);
                    ThreadMap.erase(socket);
                }

                _server->execute(
                    [=]()
                    {
                        socket->close();
                    });
                break;
            }
        });
    {
        std::lock_guard<std::mutex> lk(gMtx);
        ThreadMap[socket] = _Thread.native_handle();
    }
    _Thread.detach();
}
void MMHandler::onData(seasocks::WebSocket* socket, const char* data)
{
    std::lock_guard<std::mutex> lk(gMtx);
    if (!SocketMap.contains(socket))
        return;

    auto& socketData = SocketMap[socket];
    auto& queue = QueueMap[socketData.first][socketData.second];
    for (auto& sock : queue)
    {
        nlohmann::json msg, update_payload;
        msg["name"] = "StatusUpdate";
        update_payload["state"] = "Queued";
        update_payload["ticketId"] = TicketMap[sock];
        update_payload["queuedPlayers"] = queue.size();
        update_payload["estimatedWaitSec"] = 60 + (rand() % 15);
        update_payload["status"] = nlohmann::json();
        msg["payload"] = update_payload;
        sock->send(msg.dump());
    }
}
void MMHandler::onDisconnect(seasocks::WebSocket* socket)
{
    dropSocket(socket);
}