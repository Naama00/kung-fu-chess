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

# Build toggles (Allows building headless server in Docker without GUI dependencies)
option(BUILD_CLIENTS "Build graphical clients (SFML & OpenCV)" ON)
option(BUILD_TESTS "Build Catch2 unit tests" ON)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

if(NOT Boost_INCLUDE_DIR)
    set(Boost_INCLUDE_DIR "C:/local/boost_1_84_0")
endif()

if(NOT SFML_DIR)
    set(SFML_DIR "C:/SFML/lib/cmake/SFML")
endif()

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Portable libsodium discovery (vcpkg unofficial-sodium vs standard system libsodium)
find_package(unofficial-sodium CONFIG QUIET)
if(NOT unofficial-sodium_FOUND)
    find_library(SODIUM_LIB NAMES sodium libsodium REQUIRED)
    find_path(SODIUM_INCLUDE_DIR NAMES sodium.h REQUIRED)
    add_library(unofficial-sodium::sodium UNKNOWN IMPORTED)
    set_target_properties(unofficial-sodium::sodium PROPERTIES
        IMPORTED_LOCATION "${SODIUM_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${SODIUM_INCLUDE_DIR}"
    )
endif()

# Find GUI dependencies only if building clients
if(BUILD_CLIENTS)
    find_package(OpenCV REQUIRED)
    find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)
endif()

# Setup unit-testing framework (Catch2)
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# ------------------------------------------------------------------------------
# Source File Globbing
# ------------------------------------------------------------------------------

# Core engine and domain logic files (Supports both src/ prefix and root-level structures)
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    engine/*.cpp       src/engine/*.cpp
    players/*.cpp      src/players/*.cpp
    ui/*.cpp           src/ui/*.cpp
)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    tests/*.cpp
)

file(GLOB_RECURSE OPENCV_BACKEND_SOURCES CONFIGURE_DEPENDS
    graphics/opencv/*.cpp src/graphics/opencv/*.cpp
)

file(GLOB_RECURSE SFML_BACKEND_SOURCES CONFIGURE_DEPENDS
    graphics/sfml/*.cpp   src/graphics/sfml/*.cpp
)

# Server core implementation files (Excluding main entry points in cmd/)
file(GLOB_RECURSE SERVER_CORE_SOURCES CONFIGURE_DEPENDS
    server/network/*.cpp     src/server/network/*.cpp
    server/match/*.cpp       src/server/match/*.cpp
    server/persistence/*.cpp src/server/persistence/*.cpp
)

# ------------------------------------------------------------------------------
# Target 1: Core Engine Library
# ------------------------------------------------------------------------------

add_library(kungfu_engine ${ENGINE_SOURCES})
target_include_directories(kungfu_engine PUBLIC . src ${Boost_INCLUDE_DIR})

# ------------------------------------------------------------------------------
# Target 2: Server Core Library (Shared across all microservices)
# ------------------------------------------------------------------------------

add_library(kungfu_server_core STATIC ${SERVER_CORE_SOURCES})
target_include_directories(kungfu_server_core PUBLIC . src src/server server)

target_link_libraries(kungfu_server_core PUBLIC 
    kungfu_engine 
    SQLite::SQLite3 
    unofficial-sodium::sodium
)

# ------------------------------------------------------------------------------
# Target 3: Microservices Executables (Distributed Architecture)
# ------------------------------------------------------------------------------

# Helper logic to auto-detect whether cmd/ is placed inside src/server/cmd or server/cmd
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/server/cmd/gateway_main.cpp")
    set(CMD_DIR "src/server/cmd")
else()
    set(CMD_DIR "server/cmd")
endif()

# 3a. API & Realtime Gateway Service
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${CMD_DIR}/gateway_main.cpp")
    add_executable(gateway_service ${CMD_DIR}/gateway_main.cpp)
    target_link_libraries(gateway_service PRIVATE kungfu_server_core)
endif()

# 3b. Matchmaker & Game Allocator Service
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${CMD_DIR}/matchmaker_main.cpp")
    add_executable(matchmaker_service ${CMD_DIR}/matchmaker_main.cpp)
    target_link_libraries(matchmaker_service PRIVATE kungfu_server_core)
endif()

# 3c. Authoritative Game Server Shard Executable
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${CMD_DIR}/gameserver_main.cpp")
    add_executable(gameserver_shard ${CMD_DIR}/gameserver_main.cpp)
    target_link_libraries(gameserver_shard PRIVATE kungfu_server_core)
endif()

# 3d. Legacy Monolithic Server (For backwards compatibility/testing)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/server/main.cpp")
    add_executable(kungfu_server src/server/main.cpp)
    target_link_libraries(kungfu_server PRIVATE kungfu_server_core)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp")
    add_executable(kungfu_server server/main.cpp)
    target_link_libraries(kungfu_server PRIVATE kungfu_server_core)
endif()

# ------------------------------------------------------------------------------
# Optional Target 4: Unit Tests
# ------------------------------------------------------------------------------

if(BUILD_TESTS)
    add_executable(kungfu_tests ${TEST_SOURCES})

    target_link_libraries(kungfu_tests PRIVATE
        kungfu_engine
        kungfu_server_core
        Catch2::Catch2WithMain
    )

    enable_testing()
    add_test(NAME unit_tests COMMAND kungfu_tests)
endif()

# ------------------------------------------------------------------------------
# Optional Targets 5 & 6: Graphical Clients
# ------------------------------------------------------------------------------

if(BUILD_CLIENTS)
    # Determine OpenCV app entry path
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main_opencv.cpp")
        set(OPENCV_MAIN "src/app/main_opencv.cpp")
    else()
        set(OPENCV_MAIN "app/main_opencv.cpp")
    endif()

    add_executable(kungfu_game_opencv
        ${OPENCV_MAIN}
        ${OPENCV_BACKEND_SOURCES}
    )

    target_include_directories(kungfu_game_opencv PRIVATE
        . src ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(kungfu_game_opencv PRIVATE
        kungfu_engine
        ${OpenCV_LIBS}
    )

    # Determine SFML app entry path
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main_sfml.cpp")
        set(SFML_MAIN "src/app/main_sfml.cpp")
    else()
        set(SFML_MAIN "app/main_sfml.cpp")
    endif()

    add_executable(kungfu_game_sfml
        ${SFML_MAIN}
        ${SFML_BACKEND_SOURCES}
    )

    target_include_directories(kungfu_game_sfml PRIVATE . src)

    target_link_libraries(kungfu_game_sfml PRIVATE
        kungfu_engine
        SFML::Graphics
        SFML::Window
        SFML::System
        SFML::Audio
    )

    if(WIN32)
        file(GLOB OPENCV_DLLS "${OpenCV_DIR}/x64/vc16/bin/opencv_world*.dll")
        foreach(dll ${OPENCV_DLLS})
            add_custom_command(TARGET kungfu_game_opencv POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" $<TARGET_FILE_DIR:kungfu_game_opencv>
            )
        endforeach()

        if(SFML_DIR)
            file(GLOB SFML_DLLS "${SFML_DIR}/../../bin/sfml-*.dll" "${SFML_DIR}/bin/sfml-*.dll")
            foreach(dll ${SFML_DLLS})
                add_custom_command(TARGET kungfu_game_sfml POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" $<TARGET_FILE_DIR:kungfu_game_sfml>
                )
            endforeach()
        endif()
    endif()
endif()