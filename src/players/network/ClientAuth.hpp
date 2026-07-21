// players/network/ClientAuth.hpp
#pragma once

#include <string>

namespace kungfu {

// Structure holding the client-side session state for the active local player.
struct ClientAuth {
    inline static std::string username = "";
    inline static std::string password = "";
    inline static int rating = 0; // Updated dynamically upon LOGIN_RESPONSE from server
    inline static bool isAuthenticated = false;

    // Resets session state upon logout or disconnect
    static void reset() {
        username.clear();
        password.clear();
        rating = 0;
        isAuthenticated = false;
    }
};

} // namespace kungfu