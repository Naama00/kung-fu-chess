// server/persistence/PasswordHasher.cpp
#include "server/persistence/PasswordHasher.hpp"
#include <sodium.h>
#include <iostream>
#include <stdexcept>

namespace kungfu {

SodiumPasswordHasher::SodiumPasswordHasher() {
    if (sodium_init() < 0) {
        std::cerr << "[PasswordHasher] Critical: Libsodium failed to initialize." << std::endl;
        throw std::runtime_error("Libsodium initialization failed.");
    }
}

std::string SodiumPasswordHasher::hashPassword(const std::string& password) const {
    char hashed[crypto_pwhash_STRBYTES];

    // Compute Argon2id password hash using interactive security limits
    int result = crypto_pwhash_str(
        hashed,
        password.c_str(),
        password.length(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE);

    if (result != 0) {
        throw std::runtime_error("libsodium: Failed to generate Argon2id password hash.");
    }

    return std::string(hashed);
}

bool SodiumPasswordHasher::verifyPassword(const std::string& password, const std::string& hash) const {
    // Timing-attack resistant verification provided natively by libsodium
    return crypto_pwhash_str_verify(
               hash.c_str(),
               password.c_str(),
               password.length()) == 0;
}

} // namespace kungfu