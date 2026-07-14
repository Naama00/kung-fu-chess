#include "core/engine/PremoveQueue.hpp"
#include "core/common/Enums.hpp"
#include <algorithm>

namespace kungfu {

void PremoveQueue::registerOrUpdate(const PiecePtr& piece, const Position& to) {
    auto it = std::find_if(entries_.begin(), entries_.end(), [&](const auto& pair) {
        return pair.first == piece;
    });

    if (it != entries_.end()) {
        it->second = to;
    } else {
        entries_.push_back({piece, to});
    }
}

void PremoveQueue::cancel(const PiecePtr& piece) noexcept {
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [&](const auto& pair) { return pair.first == piece; }),
        entries_.end()
    );
}

void PremoveQueue::replacePiece(const PiecePtr& oldPiece, const PiecePtr& newPiece) noexcept {
    for (auto& entry : entries_) {
        if (entry.first == oldPiece) {
            entry.first = newPiece;
        }
    }
}

void PremoveQueue::processReady(const PieceBusyPredicate& isBusy, const MoveExecutor& execute) {
    for (auto it = entries_.begin(); it != entries_.end(); ) {
        auto piece = it->first;
        auto to = it->second;

        if (!piece || piece->state() == PieceState::Captured) {
            it = entries_.erase(it);
            continue;
        }

        if (!isBusy(piece)) {
            execute(piece->position(), to);
            it = entries_.erase(it);
            continue;
        }

        ++it;
    }
}

}  // namespace kungfu