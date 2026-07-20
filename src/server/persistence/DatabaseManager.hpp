// src/server/persistence/DatabaseManager.hpp
#pragma once

#include <string>
#include <mutex>
#include <sqlite3.h>

namespace kungfu {

// Thread-safe database management wrapper utilizing SQLite3 
// and secured cryptographically with libsodium (Argon2id).
class DatabaseManager {
private:
    sqlite3* db_ = nullptr;
    std::mutex dbMutex_;

    // Hashes a plaintext password using Argon2id via libsodium.
    std::string hashPassword(const std::string& password) const;

    // Verifies a plaintext password against a stored Argon2id hash.
    bool verifyPassword(const std::string& password, const std::string& hash) const;

public:
    DatabaseManager() = default;
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // Initializes libsodium and opens the SQLite3 file, constructing the schema.
    bool initialize(const std::string& dbPath);

    // Registers a new user with a cryptographically hashed password.
    bool registerUser(const std::string& username, const std::string& password);

    // Validates credentials and retrieves the current rating of the user.
    bool authenticateUser(const std::string& username, const std::string& password, int& outRating);

    // Updates a player's rating securely in the persistence layer.
    bool updateRating(const std::string& username, int newRating);
};

} // namespace kungfu