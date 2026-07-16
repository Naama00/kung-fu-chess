cmake_minimum_required(VERSION 3.16)
project(kung_fu_chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------- Find OpenCV --------
find_package(OpenCV REQUIRED)
# -----------------------------

# -------- Find SFML 3 --------
# איתור SFML בגרסה 3 ומעלה עם שמות הרכיבים המעודכנים באותיות גדולות
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)
# ---------------------------

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
add_library(kungfu_engine
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

target_include_directories(kungfu_engine PUBLIC src)

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

# ── Executable 1: OpenCV Game Version ─────────────────────────────────────────
add_executable(kungfu_game_opencv 
    src/main_opencv.cpp 
    src/graphics_impl/img.cpp
)
target_include_directories(kungfu_game_opencv PRIVATE src ${OpenCV_INCLUDE_DIRS})
target_link_libraries(kungfu_game_opencv PRIVATE kungfu_engine ${OpenCV_LIBS})

# ── Executable 2: SFML Game Version ───────────────────────────────────────────
add_executable(kungfu_game_sfml 
    src/main_sfml.cpp
)
target_include_directories(kungfu_game_sfml PRIVATE src)
# קישור ספריית הליבה (kungfu_engine) לצד ה-Namespaces המעודכנים של SFML 3
target_link_libraries(kungfu_game_sfml PRIVATE kungfu_engine SFML::Graphics SFML::Window SFML::System SFML::Audio)

# ── Windows Post-Build DLL Auto-Copy ─────────────────────────────────────────
if(WIN32)
    file(GLOB OPENCV_DLLS "${OpenCV_DIR}/x64/vc16/bin/opencv_world*.dll")
    foreach(DLL ${OPENCV_DLLS})
        add_custom_command(TARGET kungfu_game_opencv POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}"
                $<TARGET_FILE_DIR:kungfu_game_opencv>
            COMMENT "Copying OpenCV DLL: ${DLL} to OpenCV build folder"
        )
    endforeach()

    if(SFML_DIR)
        file(GLOB SFML_DLLS "${SFML_DIR}/../../bin/sfml-*.dll" "${SFML_DIR}/bin/sfml-*.dll")
        foreach(DLL ${SFML_DLLS})
            add_custom_command(TARGET kungfu_game_sfml POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL}"
                    $<TARGET_FILE_DIR:kungfu_game_sfml>
                COMMENT "Copying SFML DLL: ${DLL} to SFML build folder"
            )
        endforeach()
    endif()
endif()