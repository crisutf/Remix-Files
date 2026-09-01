// mm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>
#include <iostream>
#include <mutex>
#include <ctime>
#include <thread>
#include "../public/api.h"
#include "../public/utils.h"
#include "../public/global.h"
#include "../public/sha256.h"
#include "../public/gs_socket.h"
#include "../public/mm_socket.h"

#define _dt(Type, ...)  \
    void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }

_dt(MMServer, ip, port, region, playlist, maxPlayers);

int main()
{
    std::thread(
        []()
        {
            auto logger = std::make_shared<seasocks::IgnoringLogger>();
            auto server = std::make_shared<seasocks::Server>(logger);
            server->addWebSocketHandler("/mm", std::make_shared<MMHandler>(server.get()));
            server->addWebSocketHandler("/gs", std::make_shared<GSHandler>());
            server->startListening(2052);
            printf("Listening on port 2052!\n");
            server->loop();
        }).detach();

    std::thread(
        []()
        {
            httplib::Server svr;

            svr.Get("/remix/api/v1/session/:id", GetSession);

            printf("GS API listening on port 6767!\n");
            svr.listen("0.0.0.0", 6767);
        })
        .detach();

    std::mutex mtx;
    std::condition_variable cv;
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []{ return false; });
}