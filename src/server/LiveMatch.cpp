#include "LiveMatch.hpp"
#include "Serializer.hpp"
#include "NetworkSession.hpp"
#include <utility>

namespace kungfu {

void LiveMatch::setPlayers(std::shared_ptr<NetworkSession> white, std::shared_ptr<NetworkSession> black) {
    m_whiteSession = white;
    m_blackSession = black;

    if (white) {
        m_whiteUsername = white->username();
    }
    if (black) {
        m_blackUsername = black->username();
    }
}

void LiveMatch::handlePlayerMove(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet) {
    handlePlayerMoveInternal(std::move(sender), packet);
}

void LiveMatch::handlePlayerMoveInternal(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet) {
    ActionRequest request = Serializer::deserializeToRequest(packet);

    std::vector<ActionRequest> requests = { request };
    std::vector<ActionResult> results = m_engine->processActionRequests(requests);

    if (results.empty()) return;

    const ActionResult& result = results.front();
    sender->sendPacket(NetworkMessageType::MOVE_RESULT, Serializer::serializeActionResult(result));

    if (result.status == ActionStatus::Accepted) {
        auto movePayload = Serializer::serializeMovePacket(packet);

        auto white = whiteSession();
        auto black = blackSession();

        if (white) {
            white->sendPacket(NetworkMessageType::GAME_MOVE, movePayload);
        }
        if (black) {
            black->sendPacket(NetworkMessageType::GAME_MOVE, movePayload);
        }
    }
}

void LiveMatch::notifyGameOver() {
    auto white = whiteSession();
    auto black = blackSession();

    if (white) {
        white->sendPacket(NetworkMessageType::GAME_OVER, {});
    }
    if (black) {
        black->sendPacket(NetworkMessageType::GAME_OVER, {});
    }
}

void LiveMatch::onTick() {
    if (!m_engine) {
        return;
    }

    std::vector<ActionRequest> requests;
    auto result = m_engine->processActionRequests(requests);
    (void)result;
}

void LiveMatch::triggerAutoResign() {
    stop();
    notifyGameOver();
}

} // namespace kungfu
