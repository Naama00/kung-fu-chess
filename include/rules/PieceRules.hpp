// include/rules/PieceRules.hpp
#pragma once

#include "rules/IPieceRule.hpp"
#include <memory>

namespace kungfu {

class RookRule : public IPieceRule {
public:
    std::vector<Position> getLegalDestinations(const IBoard& board, const Piece& piece) const override;
};

class BishopRule : public IPieceRule {
public:
    std::vector<Position> getLegalDestinations(const IBoard& board, const Piece& piece) const override;
};

class QueenRule : public IPieceRule {
public:
    std::vector<Position> getLegalDestinations(const IBoard& board, const Piece& piece) const override;
};

class KnightRule : public IPieceRule {
public:
    std::vector<Position> getLegalDestinations(const IBoard& board, const Piece& piece) const override;
};

class KingRule : public IPieceRule {
public:
    std::vector<Position> getLegalDestinations(const IBoard& board, const Piece& piece) const override;
};

class PawnRule : public IPieceRule {
public:
    std::vector<Position> getLegalDestinations(const IBoard& board, const Piece& piece) const override;
};

class PieceRuleFactory {
public:
    // מחזיר הפנייה לחוק המתאים לסוג הכלי, תומך ברישום דינמי של חוקים (עמידה ב-OCP)
    static const IPieceRule& getRule(PieceType type) noexcept;
    static void registerRule(PieceType type, std::unique_ptr<IPieceRule> rule) noexcept;
};

}  // namespace kungfu