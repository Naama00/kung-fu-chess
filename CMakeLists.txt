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

# ספריית הליבה של המשחק - כולל מחלקת Img וקובצי ה-UI הלוגיים והפיזיים
add_library(kungfu_engine
    src/img.cpp
    src/common/Position.cpp
    src/board/Piece.cpp   
    src/board/Board.cpp
    src/io/BoardParser.cpp
    src/io/BoardPrinter.cpp
    src/input/BoardMapper.cpp
    src/input/Controller.cpp
    src/rules/PieceRules.cpp
    src/rules/RuleEngine.cpp
    src/rules/PromotionRules.cpp
    src/rules/CollisionResolver.cpp  
    src/realtime/Motion.cpp
    src/realtime/CollisionDetector.cpp 
    src/realtime/RealTimeArbiter.cpp
    src/engine/GameEngine.cpp
    src/engine/PremoveQueue.cpp
    src/view/SnapshotBuilder.cpp
)

# קישור המנוע ל-OpenCV והגדרת נתיבי כותרות (Headers)
target_link_libraries(kungfu_engine PUBLIC ${OpenCV_LIBS})
target_include_directories(kungfu_engine PUBLIC include ${OpenCV_INCLUDE_DIRS})

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
target_include_directories(kungfu_tests PRIVATE include)

enable_testing()
add_test(NAME unit_tests COMMAND kungfu_tests)

# קובץ הריצה הראשי של המשחק
add_executable(kungfu_game src/main.cpp)
target_link_libraries(kungfu_game PRIVATE kungfu_engine)
target_include_directories(kungfu_game PRIVATE include)