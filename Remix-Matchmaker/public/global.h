#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include "../public/json.hpp"
#include "server.h"
#include "seasocks/IgnoringLogger.h"
#include "seasocks/Server.h"

inline std::mutex gMtx;
inline std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::shared_ptr<MMServer>>>> ServerMap;
inline std::unordered_map<std::string, std::shared_ptr<MMServer>> ServerMapById;