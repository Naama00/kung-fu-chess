// server/persistence/PasswordHasher.hpp
#pragma once

#include <string>

namespace kungfu {

// Abstract interface for password security operations.
class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;

    virtual std::string hashPassword(const std::string& password) const = 0;
    virtual bool verifyPassword(const std::string& password, const std::string& hash) const = 0;
};

// Cryptographically secure password hasher using Libsodium (Argon2id).
class SodiumPasswordHasher : public IPasswordHasher {
public:
    SodiumPasswordHasher();
    ~SodiumPasswordHasher() override = default;

    std::string hashPassword(const std::string& password) const override;
    bool verifyPassword(const std::string& password, const std::string& hash) const override;
};

} // namespace kungfu