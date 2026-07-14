cmake_minimum_required(VERSION 3.16)
project(kung_fu_chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------- Find OpenCV --------
find_package(OpenCV REQUIRED)
# -----------------------------

# -------- Catch2 via FetchContent --------
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)
# -----------------------------------------

# ── Core game library ─────────────────────────────────────────────────────────
# All headers and sources now live under src/core/<module>/
# The graphics glue (Img) lives under src/graphics_impl/
add_library(kungfu_engine
    src/graphics_impl/img.cpp
    src/core/common/Position.cpp
    src/core/board/Piece.cpp
    src/core/board/Board.cpp
    src/core/io/BoardParser.cpp
    src/core/io/BoardPrinter.cpp
    src/core/input/BoardMapper.cpp
    src/core/input/Controller.cpp
    src/core/rules/PieceRules.cpp
    src/core/rules/RuleEngine.cpp
    src/core/rules/PromotionRules.cpp
    src/core/rules/CollisionResolver.cpp
    src/core/realtime/Motion.cpp
    src/core/realtime/CollisionDetector.cpp
    src/core/realtime/RealTimeArbiter.cpp
    src/core/engine/GameEngine.cpp
    src/core/engine/PremoveQueue.cpp
    src/core/view/SnapshotBuilder.cpp
)

# Single include root: everything is resolved relative to src/
target_include_directories(kungfu_engine PUBLIC src ${OpenCV_INCLUDE_DIRS})
target_link_libraries(kungfu_engine PUBLIC ${OpenCV_LIBS})

# ── Tests ─────────────────────────────────────────────────────────────────────
add_executable(kungfu_tests
    tests/unit/ModelTests.cpp
    tests/unit/ControllerAndMapperTests.cpp
    tests/unit/RuleEngineTests.cpp
    tests/unit/BoardIoTests.cpp
    tests/unit/PremoveQueueTests.cpp
    tests/unit/CollisionSystemTests.cpp
    tests/unit/engine/GameEngineTurnBasedTests.cpp
    tests/unit/engine/GameEngineSimultaneousTests.cpp
    tests/unit/engine/GameEnginePromotionTests.cpp
    tests/unit/engine/GameEnginePremoveTests.cpp
    tests/unit/engine/GameEngineJumpTests.cpp
    tests/integration/SimpleFlowTests.cpp
)

target_link_libraries(kungfu_tests PRIVATE kungfu_engine Catch2::Catch2WithMain)
target_include_directories(kungfu_tests PRIVATE src)

enable_testing()
add_test(NAME unit_tests COMMAND kungfu_tests)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(kungfu_game src/main.cpp)
target_link_libraries(kungfu_game PRIVATE kungfu_engine)
target_include_directories(kungfu_game PRIVATE src)

# Auto-copy OpenCV DLL next to the exe on Windows.
# OpenCV_DIR resolves to <opencv_root>/build, so the Release DLL lives at
# <opencv_root>/build/x64/vc16/bin/opencv_world*.dll
if(WIN32)
    file(GLOB OPENCV_DLLS "${OpenCV_DIR}/x64/vc16/bin/opencv_world*.dll")
    foreach(DLL ${OPENCV_DLLS})
        add_custom_command(TARGET kungfu_game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}"
                $<TARGET_FILE_DIR:kungfu_game>
            COMMENT "Copying ${DLL} to output directory"
        )
    endforeach()
endif()
