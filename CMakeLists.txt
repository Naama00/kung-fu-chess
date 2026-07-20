cmake_minimum_required(VERSION 3.16)

project(kung_fu_chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

if(MSVC)
    add_compile_options(/FS)
endif()

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

if(NOT Boost_INCLUDE_DIR)
    set(Boost_INCLUDE_DIR "C:/local/boost_1_84_0")
endif()

if(NOT SFML_DIR)
    set(SFML_DIR "C:/SFML/lib/cmake/SFML")
endif()

find_package(OpenCV REQUIRED)
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)

FetchContent_MakeAvailable(Catch2)

# ------------------------------------------------------------------------------
# Sources
# ------------------------------------------------------------------------------

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    src/engine/*.cpp
    src/players/*.cpp
)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    tests/*.cpp
)

file(GLOB_RECURSE OPENCV_BACKEND_SOURCES CONFIGURE_DEPENDS
    src/graphics/opencv/*.cpp
)

file(GLOB_RECURSE SFML_BACKEND_SOURCES CONFIGURE_DEPENDS
    src/graphics/sfml/*.cpp
)

# ------------------------------------------------------------------------------
# Engine
# ------------------------------------------------------------------------------

add_library(kungfu_engine ${ENGINE_SOURCES})

# הגדרת נתיב ברירת מחדל ל-Boost (כך שהקוד יתקמפל תמיד, גם בלי להגדיר בטרמינל)
if(NOT Boost_INCLUDE_DIR)
    set(Boost_INCLUDE_DIR "C:/local/boost_1_84_0")
endif()

# עדכון נתיבי החיפוש של המנוע
target_include_directories(kungfu_engine PUBLIC src ${Boost_INCLUDE_DIR})

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------

add_executable(kungfu_tests
    ${TEST_SOURCES}
)

target_link_libraries(kungfu_tests PRIVATE
    kungfu_engine
    Catch2::Catch2WithMain
)

target_sources(kungfu_tests PRIVATE
    src/server/NetworkSession.cpp
    src/server/DatabaseManager.cpp
    src/server/LiveMatch.cpp
    src/server/MatchManager.cpp
)

enable_testing()
add_test(NAME unit_tests COMMAND kungfu_tests)

# ------------------------------------------------------------------------------
# Server executable (ההגדרה החדשה לשרת המשחק)
# ------------------------------------------------------------------------------

add_executable(kungfu_server
    src/server/main.cpp
    src/server/NetworkSession.cpp
    src/server/DatabaseManager.cpp
    src/server/LiveMatch.cpp
    src/server/MatchManager.cpp
)

# הוספת תיקיית השרת המקומית לנתיבי החיפוש של השרת
target_include_directories(kungfu_server PRIVATE
    src/server
)

# קישור השרת למנוע המשחק (יורש אוטומטית את Boost ואת התיקייה src מתוך kungfu_engine)
target_link_libraries(kungfu_server PRIVATE
    kungfu_engine
)

# ------------------------------------------------------------------------------
# OpenCV executable
# ------------------------------------------------------------------------------

add_executable(kungfu_game_opencv
    src/app/main_opencv.cpp
    ${OPENCV_BACKEND_SOURCES}
)

target_include_directories(kungfu_game_opencv PRIVATE
    src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(kungfu_game_opencv PRIVATE
    kungfu_engine
    ${OpenCV_LIBS}
)

# ------------------------------------------------------------------------------
# SFML executable
# ------------------------------------------------------------------------------

add_executable(kungfu_game_sfml
    src/app/main_sfml.cpp
    ${SFML_BACKEND_SOURCES}
)

target_include_directories(kungfu_game_sfml PRIVATE src)

target_link_libraries(kungfu_game_sfml PRIVATE
    kungfu_engine
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
)

# ------------------------------------------------------------------------------
# Windows DLL Copy
# ------------------------------------------------------------------------------

if(WIN32)

    file(GLOB OPENCV_DLLS
        "${OpenCV_DIR}/x64/vc16/bin/opencv_world*.dll"
    )

    foreach(dll ${OPENCV_DLLS})
        add_custom_command(TARGET kungfu_game_opencv POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dll}"
                $<TARGET_FILE_DIR:kungfu_game_opencv>
        )
    endforeach()

    if(SFML_DIR)

        file(GLOB SFML_DLLS
            "${SFML_DIR}/../../bin/sfml-*.dll"
            "${SFML_DIR}/bin/sfml-*.dll"
        )

        foreach(dll ${SFML_DLLS})
            add_custom_command(TARGET kungfu_game_sfml POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${dll}"
                    $<TARGET_FILE_DIR:kungfu_game_sfml>
            )
        endforeach()

    endif()

endif()