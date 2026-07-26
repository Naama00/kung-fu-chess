// server/persistence/IUserRepository.hpp
#pragma once

#include <string>
#include <optional>
#include "server/persistence/UserRecord.hpp"

namespace kungfu {

// Abstract interface defining data persistence operations for user entities.
// Decoupled from any concrete database implementation (DIP & OCP compliant).
class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    // Initializes the storage mechanism (e.g., opens DB file or connects to server)
    virtual bool initialize(const std::string& connectionString) = 0;

    // Persists a new user record. Returns true on success, false if username exists/error occurs.
    virtual bool createUser(const UserRecord& user) = 0;

    // Retrieves a user record by unique username. Returns std::nullopt if not found.
    virtual std::optional<UserRecord> findByUsername(const std::string& username) = 0;

    // Updates a player's Elo rating. Returns true on success.
    virtual bool updateRating(const std::string& username, int newRating) = 0;
};

} // namespace kungfu