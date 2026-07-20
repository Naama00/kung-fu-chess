# CMakeLists.txt
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

# Find external visual, system and utility frameworks
find_package(OpenCV REQUIRED)
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)

# Find SQLite3 (using standard CMake integration)
find_package(SQLite3 REQUIRED)

# Find libsodium (vcpkg exports libsodium as "unofficial-sodium" in CONFIG mode)
find_package(unofficial-sodium CONFIG REQUIRED)

# Setup unit-testing framework (Catch2)
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

# ------------------------------------------------------------------------------
# Source File Globbing (Keeps build script clean and dynamically updated)
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

# Globbing all server implementation files (excluding main.cpp to prevent link errors in tests)
file(GLOB_RECURSE SERVER_CORE_SOURCES CONFIGURE_DEPENDS
    src/server/network/*.cpp
    src/server/match/*.cpp
    src/server/persistence/*.cpp
)

# ------------------------------------------------------------------------------
# Target 1: Core Engine Library
# ------------------------------------------------------------------------------

add_library(kungfu_engine ${ENGINE_SOURCES})
target_include_directories(kungfu_engine PUBLIC src ${Boost_INCLUDE_DIR})

# ------------------------------------------------------------------------------
# Target 2: Server Core Library (Decoupled Shared State - DRY Principle)
# ------------------------------------------------------------------------------

add_library(kungfu_server_core STATIC ${SERVER_CORE_SOURCES})
target_include_directories(kungfu_server_core PUBLIC src/server)

# Propagate dependencies automatically to anyone linking against the server core
# Using the unofficial-sodium::sodium target exported by vcpkg
target_link_libraries(kungfu_server_core PUBLIC 
    kungfu_engine 
    SQLite::SQLite3 
    unofficial-sodium::sodium
)

# ------------------------------------------------------------------------------
# Target 3: Server Executable (Uses the shared Server Core)
# ------------------------------------------------------------------------------

add_executable(kungfu_server src/server/main.cpp)
target_link_libraries(kungfu_server PRIVATE kungfu_server_core)

# ------------------------------------------------------------------------------
# Target 4: Game Tests Executable (Lego-like clean inclusion of Server Core)
# ------------------------------------------------------------------------------

add_executable(kungfu_tests ${TEST_SOURCES})

target_link_libraries(kungfu_tests PRIVATE
    kungfu_engine
    kungfu_server_core
    Catch2::Catch2WithMain
)

enable_testing()
add_test(NAME unit_tests COMMAND kungfu_tests)

# ------------------------------------------------------------------------------
# Target 5: OpenCV Client Executable
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
# Target 6: SFML Client Executable
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
# Windows DLL Post-Build Copy Actions
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