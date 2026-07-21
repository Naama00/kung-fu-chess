// server/match/MatchManager.cpp
#include "MatchManager.hpp"
#include "../network/NetworkSession.hpp"
#include "LiveMatch.hpp"
#include "MatchFactory.hpp"
#include "../network/Serializer.hpp"
#include "server/ServerConfig.hpp"
#include <algorithm>
#include <iostream>
#include <memory>

namespace kungfu {

MatchManager::MatchManager(boost::asio::io_context& ioContext)
    : m_ioContext(ioContext), m_matchmakingTimer(ioContext) {
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

void MatchManager::registerPlayer(std::shared_ptr<NetworkSession> session, std::uint64_t roomCode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& player : m_waitingPool) {
        if (player.session == session) return;
    }

    m_waitingPool.push_back({session, std::chrono::steady_clock::now(), session->rating(), roomCode});
    std::cout << "[Lobby] Player " << session->username()
              << " entered matchmaking. (ELO: " << session->rating()
              << ", Room Code: " << roomCode
              << "). Pool size: " << m_waitingPool.size() << std::endl;
}

void MatchManager::unregisterPlayer(std::shared_ptr<NetworkSession> session) {
    std::shared_ptr<LiveMatch> activeMatch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_waitingPool.begin(); it != m_waitingPool.end(); ++it) {
            if (it->session == session) {
                m_waitingPool.erase(it);
                break;
            }
        }
        std::uint64_t matchId = session->matchId();
        if (matchId != 0) {
            auto it = m_matches.find(matchId);
            if (it != m_matches.end()) {
                activeMatch = it->second;
            }
        }
    }
    if (activeMatch) {
        activeMatch->handlePlayerDisconnect(session);
    }
}

void MatchManager::removeTimedOutPlayers(std::chrono::steady_clock::time_point now) {
    auto it = std::remove_if(m_waitingPool.begin(), m_waitingPool.end(),
        [now](const WaitingPlayer& player) {
            auto waitDuration = std::chrono::duration_cast<std::chrono::seconds>(now - player.joinTime);
            if (waitDuration >= ServerConfig::kMatchmakingTimeout) {
                std::cout << "[Lobby] Matchmaking timeout for " << player.session->username() << std::endl;
                player.session->sendPacket(NetworkMessageType::MATCH_TIMEOUT, {});
                return true;
            }
            return false;
        });

    m_waitingPool.erase(it, m_waitingPool.end());
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

    // 1. Remove players who have waited too long
    removeTimedOutPlayers(now);

    std::vector<WaitingPlayer> remainingPool;

    // 2. Iterate through the remaining waiting pool and pair compatible players
    for (size_t i = 0; i < m_waitingPool.size(); ++i) {
        bool paired = false;
        const auto& player1 = m_waitingPool[i];
        auto waitDurationSec = std::chrono::duration_cast<std::chrono::seconds>(now - player1.joinTime).count();

        for (size_t j = i + 1; j < m_waitingPool.size(); ++j) {
            const auto& player2 = m_waitingPool[j];

            if (canPairPlayers(player1, player2, static_cast<int>(waitDurationSec))) {
                auto s1 = player1.session;
                auto s2 = player2.session;

                m_waitingPool.erase(m_waitingPool.begin() + j);
                paired = true;

                boost::asio::post(m_ioContext, [this, s1, s2]() {
                    startNewMatch(s1, s2);
                });
                break;
            }
        }

        if (!paired) {
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
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& pair : m_matches) {
        auto match = pair.second;
        if (match->isWaitingForReconnection()) {
            if (match->isWhiteDisconnected() && match->whiteUsername() == username) {
                return match;
            }
            if (match->isBlackDisconnected() && match->blackUsername() == username) {
                return match;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<LiveMatch> MatchManager::startNewMatch(std::shared_ptr<NetworkSession> player1,
                                                       std::shared_ptr<NetworkSession> player2) {
    std::uint64_t id = m_nextMatchId++;

    auto match = MatchFactory::createStandardMatch(m_ioContext, id, player1, player2);

    match->setOnMatchEnded([this](std::uint64_t finishedMatchId) {
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

    auto buildMatchFoundPayload = [id](PlayerColor color) {
        std::vector<std::uint8_t> payload;
        Serializer::writeU64(payload, id);
        Serializer::writeU8(payload, static_cast<std::uint8_t>(color));
        return payload;
    };

    player1->sendPacket(NetworkMessageType::MATCH_FOUND, buildMatchFoundPayload(PlayerColor::White));
    player2->sendPacket(NetworkMessageType::MATCH_FOUND, buildMatchFoundPayload(PlayerColor::Black));

    match->start();

    std::cout << "[MatchManager] Created match " << id << " between: "
              << player1->username() << " vs " << player2->username() << std::endl;

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
        info.blackPlayer = pair.second->blackUsername().empty() ? "AI/Opponent" : pair.second->blackUsername();
        list.push_back(info);
    }
    return list;
}

} // namespace kungfu