// server/persistence/SqliteUserRepository.hpp
#pragma once

#include "server/persistence/IUserRepository.hpp"
#include <mutex>
#include <sqlite3.h>

namespace kungfu {

// Concrete implementation of IUserRepository backed by SQLite3 database.
class SqliteUserRepository : public IUserRepository {
private:
    sqlite3* db_ = nullptr;
    std::mutex dbMutex_;

public:
    SqliteUserRepository() = default;
    ~SqliteUserRepository() override;

    SqliteUserRepository(const SqliteUserRepository&) = delete;
    SqliteUserRepository& operator=(const SqliteUserRepository&) = delete;

    bool initialize(const std::string& dbPath) override;
    bool createUser(const UserRecord& user) override;
    std::optional<UserRecord> findByUsername(const std::string& username) override;
    bool updateRating(const std::string& username, int newRating) override;
};

} // namespace kungfu