#include "players/human/HumanPlayer.hpp"
#include <utility>

namespace kungfu {

HumanPlayer::HumanPlayer(std::shared_ptr<IGameEngine> engine, int cellSize)
    : engine_(std::move(engine)), controller_(engine_, cellSize) {}

ControllerResult HumanPlayer::handleClick(int x, int y) {
    auto result = controller_.click(x, y);
    pendingRequests_.clear();

    if (result.actionTaken && !result.description.empty() && result.description != "Selection cleared" &&
        result.from.has_value() && result.to.has_value() && result.playerColor.has_value()) {
        pendingRequests_.emplace_back(
            nextRequestId_++,
            result.playerColor.value(),
            PlayerAction(result.from.value(), result.to.value()));
    }

    return result;
}

std::optional<Position> HumanPlayer::selectedPosition() const noexcept {
    return controller_.selectedPosition();
}

void HumanPlayer::clearSelection() noexcept {
    controller_.clearSelection();
}

void HumanPlayer::setCellSize(int cellSize) noexcept {
    controller_.setCellSize(cellSize);
}

std::vector<ActionRequest> HumanPlayer::decideActions(const view::GameSnapshot& snapshot) {
    (void)snapshot;
    auto requests = pendingRequests_;
    pendingRequests_.clear();
    return requests;
}

} // namespace kungfu
