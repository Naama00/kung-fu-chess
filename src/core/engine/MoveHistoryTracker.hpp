// core/engine/MoveHistoryTracker.hpp
#pragma once
#include "core/engine/IGameObserver.hpp"
#include "core/common/PieceTokenCodec.hpp"
#include "core/common/Enums.hpp"
#include <vector>
#include <string>
#include <cstdio>

namespace kungfu {

struct RecordedMove {
    PlayerColor color;
    std::string timeStr;
    std::string moveStr;
};

class MoveHistoryTracker : public IGameObserver {
private:
    std::vector<RecordedMove> moves_;

public:
    const std::vector<RecordedMove>& getMoves() const noexcept { return moves_; }
    
    void clear() noexcept { moves_.clear(); }

    void onMoveCompleted(const ArrivalEvent& event, int currentTimeMs) override {
        if (event.cancelled || !event.piece) {
            return;
        }

        RecordedMove rec;
        rec.color = event.piece->color();

        int mins = currentTimeMs / 60000;
        int secs = (currentTimeMs % 60000) / 1000;
        int ms = currentTimeMs % 1000;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%02d:%02d.%03d", mins, secs, ms);
        rec.timeStr = std::string(buf);

        std::string notation = "";
        if (event.piece->type() != PieceType::Pawn) {
            notation += PieceTokenCodec::toChar(event.piece->type());
        }
        notation += static_cast<char>('a' + event.to.col());
        notation += static_cast<char>('1' + event.to.row());
        
        if (event.capturedKing) {
            notation += "#";
        }
        rec.moveStr = notation;

        moves_.push_back(rec);
    }
};

} // namespace kungfu