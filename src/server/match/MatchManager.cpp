// src/server/match/MatchManager.cpp
#include "MatchManager.hpp"
#include "../network/NetworkSession.hpp"
#include "LiveMatch.hpp"
#include "MatchFactory.hpp"
#include "../network/Serializer.hpp"
#include <algorithm>
#include <iostream>
#include <memory>

namespace kungfu {

MatchManager::MatchManager(boost::asio::io_context& ioContext)
    : m_ioContext(ioContext), m_matchmakingTimer(ioContext) {
    scheduleMatchmakingTick();
}

void MatchManager::scheduleMatchmakingTick() {
    m_matchmakingTimer.expires_after(std::chrono::seconds(1));
    m_matchmakingTimer.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            runMatchmakingCycle();
            scheduleMatchmakingTick();
        }
    });
}

void MatchManager::registerPlayer(std::shared_ptr<NetworkSession> session) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& player : m_waitingPool) {
        if (player.session == session) return;
    }

    m_waitingPool.push_back({session, std::chrono::steady_clock::now(), session->rating()});
    std::cout << "[Lobby] Player " << session->username()
              << " entered matchmaking. (ELO: " << session->rating()
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

void MatchManager::runMatchmakingCycle() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_waitingPool.empty()) return;

    auto now = std::chrono::steady_clock::now();
    std::vector<WaitingPlayer> remainingPlayers;

    for (size_t i = 0; i < m_waitingPool.size(); ++i) {
        bool paired = false;

        auto waitDuration = std::chrono::duration_cast<std::chrono::seconds>(now - m_waitingPool[i].joinTime).count();
        if (waitDuration >= 60) {
            std::cout << "[Lobby] Matchmaking timeout for " << m_waitingPool[i].session->username() << std::endl;
            m_waitingPool[i].session->sendPacket(NetworkMessageType::MATCH_TIMEOUT, {});
            continue;
        }

        for (size_t j = i + 1; j < m_waitingPool.size(); ++j) {
            int eloDiff = std::abs(m_waitingPool[i].rating - m_waitingPool[j].rating);

            if (eloDiff <= 100) {
                auto player1 = m_waitingPool[i].session;
                auto player2 = m_waitingPool[j].session;

                m_waitingPool.erase(m_waitingPool.begin() + j);
                paired = true;

                boost::asio::post(m_ioContext, [this, player1, player2]() {
                    startNewMatch(player1, player2);
                });
                break;
            }
        }

        if (!paired) {
            remainingPlayers.push_back(m_waitingPool[i]);
        }
    }

    m_waitingPool = std::move(remainingPlayers);
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

    // Standard Match construction delegated cleanly to MatchFactory
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

} // namespace kungfu