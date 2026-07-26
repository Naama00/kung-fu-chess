// server/match/MatchManager.cpp
#include "server/match/MatchManager.hpp"
#include "server/match/LiveMatch.hpp"
#include "server/match/MatchFactory.hpp"
#include "server/network/PlayerSession.hpp"
#include "server/network/Serializer.hpp"
#include "server/persistence/InMemoryUserRepository.hpp"
#include "server/persistence/PasswordHasher.hpp"
#include "server/ServerConfig.hpp"
#include "engine/analysis/EloCalculator.hpp"
#include <algorithm>
#include <iostream>

namespace kungfu {

MatchManager::MatchManager(boost::asio::io_context& ioContext,
                             std::shared_ptr<IUserRepository> userRepo,
                             std::shared_ptr<IPasswordHasher> passwordHasher)
    : m_userRepo(userRepo ? std::move(userRepo) : std::make_shared<InMemoryUserRepository>()),
      m_passwordHasher(passwordHasher ? std::move(passwordHasher) : std::make_shared<SodiumPasswordHasher>()),
      m_ioContext(ioContext),
      m_matchmakingTimer(ioContext) {
    scheduleMatchmakingTick();
}

void MatchManager::scheduleMatchmakingTick() {
    m_matchmakingTimer.expires_after(ServerConfig::kMatchmakingTickInterval);
    m_matchmakingTimer.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            runMatchmakingCycle();
            scheduleMatchmakingTick();
        }
    });
}

void MatchManager::removeFromWaitingPool(const std::shared_ptr<PlayerSession>& session) {
    auto it = std::remove_if(m_waitingPool.begin(), m_waitingPool.end(),
        [&session](const WaitingPlayer& wp) {
            if (!wp.session || wp.session == session) return true;
            if (!session->username().empty() && wp.session->username() == session->username()) return true;
            return false;
        });
    m_waitingPool.erase(it, m_waitingPool.end());
}

std::pair<double, double> MatchManager::calculateMatchScores(const std::shared_ptr<LiveMatch>& match) const {
    if (match->isWhiteDisconnected()) return {0.0, 1.0};
    if (match->isBlackDisconnected()) return {1.0, 0.0};

    if (match->engine() && match->engine()->isGameOver()) {
        auto board = match->engine()->getBoard();
        if (board) {
            bool whiteKingAlive = false;
            bool blackKingAlive = false;
            for (const auto& piece : board->pieces()) {
                if (piece && piece->type() == PieceType::King && piece->state() != PieceState::Captured) {
                    if (piece->color() == PlayerColor::White) whiteKingAlive = true;
                    if (piece->color() == PlayerColor::Black) blackKingAlive = true;
                }
            }
            if (whiteKingAlive && !blackKingAlive) return {1.0, 0.0};
            if (!whiteKingAlive && blackKingAlive) return {0.0, 1.0};
        }
    }
    return {0.5, 0.5}; // Draw
}

void MatchManager::processMatchResultsAndElo(std::shared_ptr<LiveMatch> match) {
    if (!match) return;

    std::string whiteUser = match->whiteUsername();
    std::string blackUser = match->blackUsername();

    if (whiteUser.empty() && blackUser.empty()) return;

    auto [whiteScore, blackScore] = calculateMatchScores(match);

    auto whiteSession = match->whiteSession();
    auto blackSession = match->blackSession();

    int oldWhiteRating = whiteSession ? whiteSession->rating() : ServerConfig::kDefaultRating;
    int oldBlackRating = blackSession ? blackSession->rating() : ServerConfig::kDefaultRating;

    int newWhiteRating = EloCalculator::calculateNewRating(oldWhiteRating, oldBlackRating, whiteScore);
    int newBlackRating = EloCalculator::calculateNewRating(oldBlackRating, oldBlackRating, blackScore);

    if (whiteSession) whiteSession->setRating(newWhiteRating);
    if (blackSession) blackSession->setRating(newBlackRating);

    if (m_userRepo) {
        if (!whiteUser.empty()) {
            m_userRepo->updateRating(whiteUser, newWhiteRating);
            std::cout << "[Elo] Updated rating for " << whiteUser << ": " << oldWhiteRating << " -> " << newWhiteRating << std::endl;
        }
        if (!blackUser.empty()) {
            m_userRepo->updateRating(blackUser, newBlackRating);
            std::cout << "[Elo] Updated rating for " << blackUser << ": " << oldBlackRating << " -> " << newBlackRating << std::endl;
        }
    }
}

bool MatchManager::tryReconnectExistingMatch(const std::shared_ptr<PlayerSession>& session) {
    if (session->username().empty()) return false;

    for (const auto& pair : m_matches) {
        auto match = pair.second;
        if (!match || match->hasEnded()) continue;

        bool isWhiteReconnect = match->isWhiteDisconnected() && match->whiteUsername() == session->username();
        bool isBlackReconnect = match->isBlackDisconnected() && match->blackUsername() == session->username();
        if (isWhiteReconnect || isBlackReconnect) {
            match->reconnectPlayer(session);
            removeFromWaitingPool(session);
            std::cout << "[MatchManager] Reconnected " << session->username()
                      << " to match " << match->matchId() << std::endl;
            return true;
        }
    }
    return false;
}

void MatchManager::detachFromCurrentMatch(const std::shared_ptr<PlayerSession>& session) {
    std::uint64_t currentMatchId = session->matchId();
    if (currentMatchId != 0) {
        auto matchIt = m_matches.find(currentMatchId);
        if (matchIt != m_matches.end() && !matchIt->second->hasEnded()) {
            matchIt->second->handlePlayerDisconnect(session);
        }
        session->setMatchId(0);
    }
}

void MatchManager::registerPlayer(std::shared_ptr<PlayerSession> session, std::uint64_t roomCode) {
    if (!session) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    removeFromWaitingPool(session);

    if (tryReconnectExistingMatch(session)) {
        return;
    }

    detachFromCurrentMatch(session);

    m_waitingPool.push_back({session, std::chrono::steady_clock::now(), session->rating(), roomCode});
    std::cout << "[Lobby] Player " << (session->username().empty() ? "Guest" : session->username())
              << " entered matchmaking. (ELO: " << session->rating()
              << ", Room Code: " << roomCode
              << "). Pool size: " << m_waitingPool.size() << std::endl;
}

void MatchManager::unregisterPlayer(std::shared_ptr<PlayerSession> session) {
    if (!session) return;
    std::shared_ptr<LiveMatch> activeMatch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        removeFromWaitingPool(session);

        std::uint64_t matchId = session->matchId();
        if (matchId != 0) {
            auto matchIt = m_matches.find(matchId);
            if (matchIt != m_matches.end()) {
                activeMatch = matchIt->second;
            }
        }
    }
    if (activeMatch) {
        activeMatch->handlePlayerDisconnect(session);
    }
}

void MatchManager::removeTimedOutPlayers(std::chrono::steady_clock::time_point now) {
    std::vector<std::shared_ptr<PlayerSession>> timedOutSessions;

    auto it = std::remove_if(m_waitingPool.begin(), m_waitingPool.end(),
        [&timedOutSessions, now](const WaitingPlayer& player) {
            auto waitDuration = std::chrono::duration_cast<std::chrono::seconds>(now - player.joinTime);
            if (waitDuration >= ServerConfig::kMatchmakingTimeout) {
                if (player.session) {
                    timedOutSessions.push_back(player.session);
                }
                return true;
            }
            return false;
        });

    m_waitingPool.erase(it, m_waitingPool.end());

    for (const auto& session : timedOutSessions) {
        if (session) {
            std::cout << "[Lobby] Matchmaking timeout for " << session->username() << std::endl;
            session->sendPacket(NetworkMessageType::MATCH_TIMEOUT, {});
        }
    }
}

bool MatchManager::isPrivateRoomMatch(const WaitingPlayer& p1, const WaitingPlayer& p2) const {
    if (p1.roomCode != 0 || p2.roomCode != 0) {
        return p1.roomCode == p2.roomCode;
    }
    return false;
}

bool MatchManager::isRatedMatch(const WaitingPlayer& p1, const WaitingPlayer& p2, int waitDurationSec) const {
    if (p1.roomCode != 0 || p2.roomCode != 0) {
        return false;
    }

    int eloDiff = std::abs(p1.rating - p2.rating);
    int maxAllowedDiff = ServerConfig::kBaseEloDiff + (waitDurationSec * ServerConfig::kEloDiffExpansionPerSec);

    return eloDiff <= maxAllowedDiff;
}

bool MatchManager::canPairPlayers(const WaitingPlayer& p1, const WaitingPlayer& p2, int waitDurationSec) const {
    if (p1.roomCode != 0 || p2.roomCode != 0) {
        return isPrivateRoomMatch(p1, p2);
    }
    return isRatedMatch(p1, p2, waitDurationSec);
}

void MatchManager::runMatchmakingCycle() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_waitingPool.empty()) return;

    auto now = std::chrono::steady_clock::now();

    removeTimedOutPlayers(now);

    if (m_waitingPool.empty()) return;

    std::vector<bool> matched(m_waitingPool.size(), false);
    std::vector<WaitingPlayer> remainingPool;

    for (size_t i = 0; i < m_waitingPool.size(); ++i) {
        if (matched[i]) continue;

        const auto& player1 = m_waitingPool[i];
        auto waitDurationSec = std::chrono::duration_cast<std::chrono::seconds>(now - player1.joinTime).count();

        for (size_t j = i + 1; j < m_waitingPool.size(); ++j) {
            if (matched[j]) continue;

            const auto& player2 = m_waitingPool[j];

            if (canPairPlayers(player1, player2, static_cast<int>(waitDurationSec))) {
                auto s1 = player1.session;
                auto s2 = player2.session;

                matched[i] = true;
                matched[j] = true;

                boost::asio::post(m_ioContext, [this, s1, s2]() {
                    startNewMatch(s1, s2);
                });
                break;
            }
        }

        if (!matched[i]) {
            remainingPool.push_back(player1);
        }
    }

    m_waitingPool = std::move(remainingPool);
}

std::shared_ptr<LiveMatch> MatchManager::getMatch(std::uint64_t matchId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_matches.find(matchId);
    if (it != m_matches.end()) {
        return it->second;
    }
    return nullptr;
}

void MatchManager::removeMatch(std::uint64_t matchId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_matches.erase(matchId) > 0) {
        std::cout << "[MatchManager] Match " << matchId << " removed after completion." << std::endl;
    }
}

std::shared_ptr<LiveMatch> MatchManager::findActiveMatchForUser(const std::string& username) {
    if (username.empty()) return nullptr;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& pair : m_matches) {
        auto match = pair.second;
        if (match && !match->hasEnded()) {
            bool isWhiteAndDisconnected = match->isWhiteDisconnected() && match->whiteUsername() == username;
            bool isBlackAndDisconnected = match->isBlackDisconnected() && match->blackUsername() == username;
            if (isWhiteAndDisconnected || isBlackAndDisconnected) {
                return match;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<LiveMatch> MatchManager::startNewMatch(std::shared_ptr<PlayerSession> player1,
                                                       std::shared_ptr<PlayerSession> player2) {
    std::uint64_t id = m_nextMatchId++;

    auto match = MatchFactory::createStandardMatch(m_ioContext, id, player1, player2);

    match->setOnMatchEnded([this, match](std::uint64_t finishedMatchId) {
        processMatchResultsAndElo(match);
        removeMatch(finishedMatchId);
    });

    player1->setMatchId(id);
    player1->setColor(PlayerColor::White);

    player2->setMatchId(id);
    player2->setColor(PlayerColor::Black);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_matches[id] = match;
    }

    std::string p1Name = player1->username().empty() ? "Player 1" : player1->username();
    std::string p2Name = player2->username().empty() ? "Player 2" : player2->username();

    player1->sendPacket(NetworkMessageType::MATCH_FOUND, 
        Serializer::serializeMatchFound(id, static_cast<std::uint8_t>(PlayerColor::White), p2Name, player2->rating()));
        
    player2->sendPacket(NetworkMessageType::MATCH_FOUND, 
        Serializer::serializeMatchFound(id, static_cast<std::uint8_t>(PlayerColor::Black), p1Name, player1->rating()));

    match->start();

    std::cout << "[MatchManager] Created match " << id << " between: "
              << p1Name << " vs " << p2Name << std::endl;

    return match;
}

std::vector<MatchInfo> MatchManager::getActiveMatchesList() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MatchInfo> list;
    list.reserve(m_matches.size());
    
    for (const auto& pair : m_matches) {
        MatchInfo info;
        info.matchId = pair.first;
        info.whitePlayer = pair.second->whiteUsername().empty() ? "Guest" : pair.second->whiteUsername();
        info.blackPlayer = pair.second->blackUsername().empty() ? "Guest/Opponent" : pair.second->blackUsername();
        list.push_back(info);
    }
    return list;
}

} // namespace kungfu