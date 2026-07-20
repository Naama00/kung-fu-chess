// src/server/persistence/DatabaseManager.cpp
#include "DatabaseManager.hpp"
#include <sodium.h>
#include <iostream>
#include <stdexcept>

namespace kungfu {

DatabaseManager::~DatabaseManager() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

std::string DatabaseManager::hashPassword(const std::string& password) const {
    char hashed[crypto_pwhash_STRBYTES];

    // Compute Argon2id password hash using interactive security limits.
    int result = crypto_pwhash_str(
        hashed,
        password.c_str(),
        password.length(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE
    );

    if (result != 0) {
        throw std::runtime_error("libsodium: Failed to generate Argon2id password hash.");
    }

    return std::string(hashed);
}

bool DatabaseManager::verifyPassword(const std::string& password, const std::string& hash) const {
    // Timing-attack resistant verification provided natively by libsodium.
    return crypto_pwhash_str_verify(
        hash.c_str(),
        password.c_str(),
        password.length()
    ) == 0;
}

bool DatabaseManager::initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    // Initialize libsodium library
    if (sodium_init() < 0) {
        std::cerr << "[Database] Critical: libsodium failed to initialize." << std::endl;
        return false;
    }

    // Open connection to SQLite3 database file
    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // Prepare table schema utilizing prepared executions
    const char* createTableSQL = 
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY, "
        "password_hash TEXT NOT NULL, "
        "rating INTEGER DEFAULT 1200"
        ");";

    char* errMsg = nullptr;
    rc = sqlite3_exec(db_, createTableSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Schema creation failed: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    std::cout << "[Database] Secure SQLite3 persistence initialized successfully." << std::endl;
    return true;
}

bool DatabaseManager::registerUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    std::string hashedPass;
    try {
        hashedPass = hashPassword(password);
    } catch (const std::exception& e) {
        std::cerr << "[Database] Register hashing failed: " << e.what() << std::endl;
        return false;
    }

    // Insert user credentials using standard parameterized queries
    const char* insertSQL = "INSERT INTO users (username, password_hash) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db_, insertSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Prepared insertion statement failed: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashedPass.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        // Fails with constraint violation if the username is already registered.
        return false;
    }

    std::cout << "[Database] Registered new user: " << username << std::endl;
    return true;
}

bool DatabaseManager::authenticateUser(const std::string& username, const std::string& password, int& outRating) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* selectSQL = "SELECT password_hash, rating FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db_, selectSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return false; 
    }

    const unsigned char* hashText = sqlite3_column_text(stmt, 0);
    std::string storedHash(reinterpret_cast<const char*>(hashText));
    outRating = sqlite3_column_int(stmt, 1);

    sqlite3_finalize(stmt);

    // Verify using secure timing-attack immune comparison
    return verifyPassword(password, storedHash);
}

bool DatabaseManager::updateRating(const std::string& username, int newRating) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* updateSQL = "UPDATE users SET rating = ? WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db_, updateSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, newRating);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

} // namespace kungfu