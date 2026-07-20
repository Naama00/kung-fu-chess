#pragma once

#include <string>

namespace kungfu {

struct ClientAuth {
    inline static std::string username = "";
    inline static std::string password = "";
    inline static int rating = 1200;
    inline static bool isAuthenticated = false;
};

} // namespace kungfu
