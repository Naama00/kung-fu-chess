// players/network/ClientAuth.hpp
#pragma once

#include <string>

namespace kungfu {

// Structure holding the client-side session state.
// Relocated from the server folder to prevent global state conflicts on the server.
struct ClientAuth {
    inline static std::string username = "";
    inline static std::string password = "";
    inline static int rating = 1200;
    inline static bool isAuthenticated = false;
};

} // namespace kungfu