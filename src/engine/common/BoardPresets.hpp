#pragma once
#include <string_view>

namespace kungfu {

class BoardPresets {
public:
    static constexpr std::string_view kStandardStartBoard =
        "bR bN bB bQ bK bB bN bR\n"
        "bP bP bP bP bP bP bP bP\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        "wP wP wP wP wP wP wP wP\n"
        "wR wN wB wQ wK wB wN wR\n";
};

} // namespace kungfu