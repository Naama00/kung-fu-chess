#include "players/ai/AIPlayer.hpp"

#include <utility>

namespace kungfu {

AIPlayer::AIPlayer(PlayerColor playerColor, std::mt19937::result_type seed)
    : playerColor_(playerColor), rng_(seed) {}

std::vector<ActionRequest> AIPlayer::decideActions(const view::GameSnapshot& snapshot) {
    const auto legalMoves = moveGenerator_.generateForPlayer(snapshot, playerColor_);
    if (legalMoves.empty()) {
        return {};
    }

    std::uniform_int_distribution<std::size_t> dist(0, legalMoves.size() - 1);
    const auto selected = legalMoves[dist(rng_)];

    return {ActionRequest(nextRequestId_++, playerColor_, PlayerAction(selected.from, selected.to))};
}

} // namespace kungfu
