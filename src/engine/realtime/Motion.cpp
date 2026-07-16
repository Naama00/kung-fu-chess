#include "engine/realtime/Motion.hpp"

namespace kungfu {

Motion::Motion(PiecePtr piece, const Position& from, const Position& to, int startTimeMs, int durationMs) noexcept
    : piece_(std::move(piece)), from_(from), to_(to), startTimeMs_(startTimeMs), arrivalTimeMs_(startTimeMs + durationMs) {}

}  // namespace kungfu