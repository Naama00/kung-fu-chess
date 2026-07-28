// server/persistence/PostgresUserRepository.hpp
#pragma once

#include "server/persistence/IUserRepository.hpp"
#include <mutex>
#include <string>
#include <optional>
#include <memory>
#include <pqxx/pqxx>

namespace kungfu {

/**
 * @class PostgresUserRepository
 * @brief Production-grade PostgreSQL implementation of IUserRepository using libpqxx.
 * 
 * Provides thread-safe, transactional user data persistence for authentication,
 * password hash storage (Argon2id), and ELO rating updates.
 */
class PostgresUserRepository : public IUserRepository {
private:
    std::string m_connectionString;
    mutable std::mutex m_mutex;

    /**
     * @brief Establishes a connection instance to PostgreSQL.
     * @return Pointer to open connection, or nullptr on failure.
     */
    std::unique_ptr<pqxx::connection> createConnection() const;

public:
    PostgresUserRepository() = default;
    ~PostgresUserRepository() override = default;

    PostgresUserRepository(const PostgresUserRepository&) = delete;
    PostgresUserRepository& operator=(const PostgresUserRepository&) = delete;

    /**
     * @brief Initializes database connection and verifies table/index schema.
     * @param connectionString Connection string formatted for libpqxx 
     *        (e.g., "host=localhost port=5432 dbname=kungfu user=postgres password=secret").
     * @return True if database connection and schema verification succeed.
     */
    bool initialize(const std::string& connectionString) override;

    /**
     * @brief Inserts a new user record into the database using parameterized query.
     * @param user UserRecord DTO containing username, passwordHash, and initial rating.
     * @return True if user was inserted successfully, false if username exists or on DB error.
     */
    bool createUser(const UserRecord& user) override;

    /**
     * @brief Fetches a user record by username.
     * @param username Target player's unique username.
     * @return std::optional<UserRecord> containing account data if found, std::nullopt otherwise.
     */
    std::optional<UserRecord> findByUsername(const std::string& username) override;

    /**
     * @brief Atomically updates a user's ELO rating.
     * @param username Target player's username.
     * @param newRating Calculated new ELO score.
     * @return True if rating update was committed.
     */
    bool updateRating(const std::string& username, int newRating) override;
};

} // namespace kungfu