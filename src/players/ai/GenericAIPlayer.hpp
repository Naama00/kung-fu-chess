// players/ai/GenericAIPlayer.hpp
#pragma once

#include "players/IPlayer.hpp"
#include "players/ai/IAIDecisionStrategy.hpp"
#include <memory>
#include <utility>

namespace kungfu {

class GenericAIPlayer : public IPlayer {
private:
    PlayerColor playerColor_;
    std::unique_ptr<IAIDecisionStrategy> strategy_;
    std::uint64_t nextRequestId_ = 1000;

public:
    GenericAIPlayer(PlayerColor color, std::unique_ptr<IAIDecisionStrategy> strategy)
        : playerColor_(color), strategy_(std::move(strategy)) {}

    ~GenericAIPlayer() override = default;

    std::vector<ActionRequest> decideActions(const view::GameSnapshot& snapshot) override {
        if (!strategy_) return {};

        auto actions = strategy_->computeActions(snapshot, playerColor_);
        
        // Define a unique request identifier and uniform color authority for each returned move
        for (auto& req : actions) {
            req.requestId = nextRequestId_++;
            req.playerColor = playerColor_;
        }

        return actions;
    }
};

} // namespace kungfu