// server/persistence/SqliteUserRepository.cpp
#include "server/persistence/SqliteUserRepository.hpp"
#include "server/ServerConfig.hpp"
#include <iostream>

namespace kungfu {

SqliteUserRepository::~SqliteUserRepository() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SqliteUserRepository::initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Cannot open SQLite database: " << sqlite3_errmsg(db_) << std::endl;
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    std::string createTableSQL =
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY, "
        "password_hash TEXT NOT NULL, "
        "rating INTEGER DEFAULT " +
        std::to_string(ServerConfig::kDefaultRating) +
        ");";

    char* errMsg = nullptr;
    rc = sqlite3_exec(db_, createTableSQL.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Schema creation failed: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    std::cout << "[Database] SQLite repository initialized successfully at " << dbPath << std::endl;
    return true;
}

bool SqliteUserRepository::createUser(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    const char* insertSQL = "INSERT INTO users (username, password_hash, rating) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db_, insertSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Prepared insertion statement failed: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user.passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, user.rating);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return false; // Unique constraint violation or database error
    }

    return true;
}

std::optional<UserRecord> SqliteUserRepository::findByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return std::nullopt;

    const char* selectSQL = "SELECT username, password_hash, rating FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db_, selectSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    UserRecord record;
    record.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    record.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    record.rating = sqlite3_column_int(stmt, 2);

    sqlite3_finalize(stmt);
    return record;
}

bool SqliteUserRepository::updateRating(const std::string& username, int newRating) {
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