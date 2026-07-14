#pragma once

#include <memory>
#include "core/board/IBoard.hpp"
#include "core/common/Position.hpp"
#include <string>

namespace kungfu {

struct MoveValidation {
    bool isValid;
    std::string reason;
};

class RuleEngine {
public:
    explicit RuleEngine(std::shared_ptr<IBoard> board) noexcept;

    // בודק חוקיות מלאה של מהלך עבור הלוח הנוכחי ומחזיר שגיאות מובנות
    MoveValidation validateMove(const Position& from, const Position& to) const;

private:
    std::shared_ptr<IBoard> board_;
};

}  // namespace kungfu