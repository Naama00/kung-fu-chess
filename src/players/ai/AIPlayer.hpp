#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "engine/actions/ActionRequest.hpp"
#include "engine/analysis/MoveGenerator.hpp"
#include "engine/common/Position.hpp"
#include "engine/snapshot/GameSnapshot.hpp"
#include "players/IPlayer.hpp"

namespace kungfu {

class AIPlayer : public IPlayer {
public:
    explicit AIPlayer(PlayerColor playerColor, std::mt19937::result_type seed = 0xC0FFEEu);

    std::vector<ActionRequest> decideActions(const view::GameSnapshot& snapshot) override;

private:
    PlayerColor playerColor_;
    std::mt19937 rng_;
    MoveGenerator moveGenerator_;
    std::uint64_t nextRequestId_ = 1;
};

} // namespace kungfu
