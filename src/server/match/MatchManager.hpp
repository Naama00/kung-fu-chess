// server/match/MatchManager.hpp
#pragma once

#include <boost/asio.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <chrono>
#include <utility>
#include <atomic>
#include "server/persistence/IUserRepository.hpp"
#include "server/persistence/PasswordHasher.hpp"
#include "server/network/NetworkMessages.hpp"
#include "server/match/DistributedMatchmaker.hpp"
#include "server/match/RedisSessionRegistry.hpp"

namespace kungfu {

class PlayerSession;
class LiveMatch;

struct MatchInfo {
    std::uint64_t matchId;
    std::string whitePlayer;
    std::string blackPlayer;
};

class MatchManager {
public:
    struct WaitingPlayer {
        std::shared_ptr<PlayerSession> session;
        std::chrono::steady_clock::time_point joinTime;
        int rating;
        std::uint64_t roomCode = 0;
    };

private:
    std::unordered_map<std::uint64_t, std::shared_ptr<LiveMatch>> m_matches;
    
    // Atomic match counter for lock-free ID generation
    std::atomic<std::uint64_t> m_nextMatchId{1};
    mutable std::mutex m_mutex;

    std::vector<WaitingPlayer> m_waitingPool;

    std::shared_ptr<IUserRepository> m_userRepo;
    std::shared_ptr<IPasswordHasher> m_passwordHasher;

    boost::asio::io_context& m_ioContext;
    boost::asio::steady_timer m_matchmakingTimer;

    std::shared_ptr<DistributedMatchmaker> m_distributedMatchmaker;
    std::shared_ptr<RedisSessionRegistry> m_sessionRegistry;

public:
    explicit MatchManager(boost::asio::io_context& ioContext,
                         std::shared_ptr<IUserRepository> userRepo = nullptr,
                         std::shared_ptr<IPasswordHasher> passwordHasher = nullptr);

    std::shared_ptr<IUserRepository> userRepository() const { return m_userRepo; }
    std::shared_ptr<IPasswordHasher> passwordHasher() const { return m_passwordHasher; }

    void registerPlayer(std::shared_ptr<PlayerSession> session, std::uint64_t roomCode = 0);
    void unregisterPlayer(std::shared_ptr<PlayerSession> session);

    std::shared_ptr<LiveMatch> getMatch(std::uint64_t matchId);
    void removeMatch(std::uint64_t matchId);
    std::shared_ptr<LiveMatch> findActiveMatchForUser(const std::string& username);
    std::vector<MatchInfo> getActiveMatchesList();
    
    void enableDistributedMode(const std::string& redisHost, std::uint16_t redisPort) {
        m_distributedMatchmaker = std::make_shared<DistributedMatchmaker>();
        m_distributedMatchmaker->initialize(redisHost, redisPort);

        m_sessionRegistry = std::make_shared<RedisSessionRegistry>();
        m_sessionRegistry->initialize(redisHost, redisPort);
    }

private:
    void scheduleMatchmakingTick();
    void runMatchmakingCycle();
    void processMatchResultsAndElo(std::shared_ptr<LiveMatch> match);

    // Private thread-safe internal helpers (assumes caller holds lock if specified)
    void removeFromWaitingPoolInternal(const std::shared_ptr<PlayerSession>& session);
    
    bool tryReconnectExistingMatch(const std::shared_ptr<PlayerSession>& session);
    void detachFromCurrentMatch(const std::shared_ptr<PlayerSession>& session);
    std::pair<double, double> calculateMatchScores(const std::shared_ptr<LiveMatch>& match) const;

    bool canPairPlayers(const WaitingPlayer& p1, const WaitingPlayer& p2, int waitDurationSec) const;
    bool isPrivateRoomMatch(const WaitingPlayer& p1, const WaitingPlayer& p2) const;
    bool isRatedMatch(const WaitingPlayer& p1, const WaitingPlayer& p2, int waitDurationSec) const;

    std::shared_ptr<LiveMatch> startNewMatch(std::shared_ptr<PlayerSession> player1,
                                             std::shared_ptr<PlayerSession> player2);
};

} // namespace kungfu