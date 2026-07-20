#include "MatchManager.hpp"
#include "NetworkSession.hpp"
#include "LiveMatch.hpp"
#include "Serializer.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/rules/RuleEngine.hpp"
#include "engine/common/GameConfig.hpp"
#include <algorithm>
#include <iostream>
#include <memory>

namespace kungfu {

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

std::shared_ptr<LiveMatch> MatchManager::startNewMatch(std::shared_ptr<NetworkSession> player1,
                                                       std::shared_ptr<NetworkSession> player2) {
    std::uint64_t id = m_nextMatchId++;

    static const std::string kStartBoard =
        "bR bN bB bQ bK bB bN bR\n"
        "bP bP bP bP bP bP bP bP\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        "wP wP wP wP wP wP wP wP\n"
        "wR wN wB wQ wK wB wN wR\n";

    auto board = BoardParser::parse(kStartBoard);
    auto ruleEngine = std::make_shared<RuleEngine>(board);

    GameConfig config;
    config.allowSimultaneousMovement = true;
    config.enablePremoves = true;
    config.allowJumping = true;

    auto engine = std::make_shared<GameEngine>(board, ruleEngine, config);
    auto match = std::make_shared<LiveMatch>(m_ioContext, id, engine);

    match->setOnMatchEnded([this](std::uint64_t finishedMatchId) {
        removeMatch(finishedMatchId);
    });

    player1->setMatchId(id);
    player1->setColor(PlayerColor::White);

    player2->setMatchId(id);
    player2->setColor(PlayerColor::Black);

    match->setPlayers(player1, player2);

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
