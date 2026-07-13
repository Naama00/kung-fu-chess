// include/common/MoveResult.hpp
#pragma once
#include <string>

namespace kungfu {

struct MoveResult {
    bool isAccepted;
    std::string reason;
};

} // namespace kungfu