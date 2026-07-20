// server/DatabaseManager.cpp
#include "DatabaseManager.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <functional>

namespace kungfu {

DatabaseManager::~DatabaseManager() = default;

std::string DatabaseManager::hashPassword(const std::string& password) const {
    // לצורך פשטות ואי-תלות בספריות חיצוניות (כמו OpenSSL), נשתמש ב-std::hash.
    // בסביבת ייצור אמיתית נשתמש ב-SHA-256 או bcrypt.
    std::size_t h = std::hash<std::string>{}(password);
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << h;
    return oss.str();
}

bool DatabaseManager::initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    (void)dbPath;
    users_.clear();
    std::cout << "[DB] Database initialized successfully. File: " << dbPath << std::endl;
    return true;
}

bool DatabaseManager::registerUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    if (users_.find(username) != users_.end()) {
        return false;
    }

    UserRecord record;
    record.passwordHash = hashPassword(password);
    users_[username] = record;

    std::cout << "[DB] Registered user: " << username << " with ELO 1200" << std::endl;
    return true;
}

bool DatabaseManager::authenticateUser(const std::string& username, const std::string& password, int& outRating) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }

    if (it->second.passwordHash == hashPassword(password)) {
        outRating = it->second.rating;
        return true;
    }

    return false;
}

bool DatabaseManager::updateRating(const std::string& username, int newRating) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }

    it->second.rating = newRating;
    return true;
}

} // namespace kungfu