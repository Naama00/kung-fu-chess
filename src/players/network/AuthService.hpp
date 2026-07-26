// players/network/AuthService.hpp
#pragma once

#include "players/network/NetworkSession.hpp"
#include <memory>
#include <string>

namespace kungfu {

// High-level structure holding the result of authentication operations
struct AuthResult {
    bool success = false;
    std::string message;
    int rating = static_cast<int>(ClientConfig::kDefaultRating); 
};

// Application Service encapsulating user authentication and session creation.
// Fully headless with zero UI framework dependencies.
class AuthService {
public:
    AuthService() = default;
    ~AuthService() = default;

    // Synchronously performs network authentication or registration requests over TCP.
    // Intended to be invoked inside asynchronous tasks (e.g., std::async).
    AuthResult authenticate(const std::string& username,
                            const std::string& password,
                            bool isRegister,
                            const std::string& host = "127.0.0.1",
                            const std::string& port = "8080");

    // Constructs, initializes, and spins up a thread-managed, authenticated NetworkSession
    // ready for online play.
    std::shared_ptr<NetworkSession> createAuthenticatedSession(const std::string& username,
                                                               const std::string& password,
                                                               const std::string& host = ClientConfig::kDefaultHost,
                                                               const std::string& port = ClientConfig::kDefaultPort);
};

} // namespace kungfu