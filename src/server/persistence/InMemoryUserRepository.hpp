// server/persistence/InMemoryUserRepository.hpp
#pragma once

#include "server/persistence/IUserRepository.hpp"
#include <unordered_map>
#include <mutex>

namespace kungfu {

// Thread-safe In-Memory repository implementation for unit testing and mocking.
class InMemoryUserRepository : public IUserRepository {
private:
    std::unordered_map<std::string, UserRecord> users_;
    mutable std::mutex mutex_;

public:
    bool initialize(const std::string& /*connectionString*/) override {
        return true;
    }

    bool createUser(const UserRecord& user) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (users_.find(user.username) != users_.end()) {
            return false;
        }
        users_[user.username] = user;
        return true;
    }

    std::optional<UserRecord> findByUsername(const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(username);
        if (it != users_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool updateRating(const std::string& username, int newRating) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(username);
        if (it != users_.end()) {
            it->second.rating = newRating;
            return true;
        }
        return false;
    }
};

} // namespace kungfu