cmake_minimum_required(VERSION 3.16)
project(kung_fu_chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(kungfu_engine
    src/common/Position.cpp
    src/pieces/Piece.cpp
    src/board/Board.cpp
    src/io/BoardParser.cpp
    src/io/BoardPrinter.cpp
    src/input/BoardMapper.cpp
    src/input/Controller.cpp
    src/rules/PieceRules.cpp
    src/rules/RuleEngine.cpp
    src/realtime/Motion.cpp
    src/realtime/RealTimeArbiter.cpp
    src/engine/GameEngine.cpp
)
target_include_directories(kungfu_engine PUBLIC include)

add_executable(kungfu_tests
    tests/unit/ModelTests.cpp
    tests/unit/ControllerAndMapperTests.cpp
    tests/unit/RuleEngineTests.cpp
    tests/unit/GameEngineAndRealTimeTests.cpp
    tests/integration/SimpleFlowTests.cpp
    tests/unit/BoardIoTests.cpp
)
target_link_libraries(kungfu_tests PRIVATE kungfu_engine)
target_include_directories(kungfu_tests PRIVATE include)

enable_testing()
add_test(NAME unit_tests COMMAND kungfu_tests)
