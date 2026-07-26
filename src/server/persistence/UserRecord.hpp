// server/persistence/UserRecord.hpp
#pragma once

#include <string>
#include "server/ServerConfig.hpp"

namespace kungfu {

// Data Transfer Object representing a user entity in the persistence layer.
struct UserRecord {
    std::string username;
    std::string passwordHash;
    int rating = ServerConfig::kDefaultRating;
};

} // namespace kungfu