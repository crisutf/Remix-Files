#include "../public/httplib.h"
#include "../public/utils.h"
#include "../public/global.h"

void GetSession(const httplib::Request& req, httplib::Response& res)
{
    if (req.get_header_value("X-Api-Key") != "74993bcef253a6eea767ab01621b0c81")
    {
        res.status = httplib::StatusCode::Forbidden_403;
        return;
    }

    std::lock_guard<std::mutex> lk(gMtx);
    auto& sessionId = req.path_params.at("id");
    auto& srv = ServerMapById[sessionId];

    if (!srv)
        return;

    auto response = nlohmann::json(*srv).dump();
    res.set_content(response, "application/json");
}