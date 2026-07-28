// server/persistence/PostgresUserRepository.cpp
#include "server/persistence/PostgresUserRepository.hpp"
#include "server/ServerConfig.hpp"
#include <iostream>

namespace kungfu {

std::unique_ptr<pqxx::connection> PostgresUserRepository::createConnection() const {
    try {
        return std::make_unique<pqxx::connection>(m_connectionString);
    } catch (const std::exception& e) {
        std::cerr << "[PostgreSQL] Connection failed: " << e.what() << std::endl;
        return nullptr;
    }
}

bool PostgresUserRepository::initialize(const std::string& connectionString) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectionString = connectionString;

    auto conn = createConnection();
    if (!conn || !conn->is_open()) {
        std::cerr << "[PostgreSQL] Initialization failed: Unable to connect to server." << std::endl;
        return false;
    }

    try {
        pqxx::work txn(*conn);

        // Ensure users table exists with required constraints
        std::string createTableSQL =
            "CREATE TABLE IF NOT EXISTS users ("
            "username VARCHAR(64) PRIMARY KEY, "
            "password_hash VARCHAR(255) NOT NULL, "
            "rating INTEGER DEFAULT " +
            std::to_string(ServerConfig::kDefaultRating) +
            ");";

        txn.exec(createTableSQL);

        // Create secondary index on rating for fast leaderboards queries
        txn.exec("CREATE INDEX IF NOT EXISTS idx_users_rating ON users (rating DESC);");

        txn.commit();
        std::cout << "[PostgreSQL] Repository initialized and schema verified successfully." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PostgreSQL] Schema initialization failed: " << e.what() << std::endl;
        return false;
    }
}

bool PostgresUserRepository::createUser(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto conn = createConnection();
    if (!conn || !conn->is_open()) return false;

    try {
        pqxx::work txn(*conn);

        // Parameterized statement prevents SQL injection attacks
        txn.exec_params(
            "INSERT INTO users (username, password_hash, rating) VALUES ($1, $2, $3);",
            user.username,
            user.passwordHash,
            user.rating
        );

        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PostgreSQL] Failed to create user '" << user.username << "': " << e.what() << std::endl;
        return false;
    }
}

std::optional<UserRecord> PostgresUserRepository::findByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto conn = createConnection();
    if (!conn || !conn->is_open()) return std::nullopt;

    try {
        pqxx::nontransaction txn(*conn);

        pqxx::result result = txn.exec_params(
            "SELECT username, password_hash, rating FROM users WHERE username = $1;",
            username
        );

        if (result.empty()) {
            return std::nullopt;
        }

        const auto& row = result[0];
        UserRecord user;
        user.username = row["username"].as<std::string>();
        user.passwordHash = row["password_hash"].as<std::string>();
        user.rating = row["rating"].as<int>();

        return user;
    } catch (const std::exception& e) {
        std::cerr << "[PostgreSQL] Error finding user '" << username << "': " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool PostgresUserRepository::updateRating(const std::string& username, int newRating) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto conn = createConnection();
    if (!conn || !conn->is_open()) return false;

    try {
        pqxx::work txn(*conn);

        pqxx::result result = txn.exec_params(
            "UPDATE users SET rating = $1 WHERE username = $2;",
            newRating,
            username
        );

        txn.commit();

        return result.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::cerr << "[PostgreSQL] Failed to update rating for '" << username << "': " << e.what() << std::endl;
        return false;
    }
}

} // namespace kungfu