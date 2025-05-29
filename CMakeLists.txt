
cmake_minimum_required(VERSION 3.14)
project(CnCNet_ELO LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
        message(FATAL_ERROR "GCC version must be at least 12. Detected version: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
endif()

set(SOURCES
    blitzmap.cpp
    databaseconnection.cpp
    main.cpp
    faction.cpp
    game.cpp
    gamemode.cpp
    gameoverlay.cpp
    gametype.cpp
    knownplayers.cpp
    logging.cpp
    mapstats.cpp
    options.cpp
    player.cpp
    players.cpp
    probabilities.cpp
    rating.cpp
    stringtools.cpp
)

set(HEADERS
    blitzmap.h
    cplusplus.h
    databaseconnection.h
    faction.h
    game.h
    gamemode.h
    gameoverlay.h
    gametype.h
    knownplayers.h
    logging.h
    mapstats.h
    options.h
    player.h
    players.h
    probabilities.h
    rating.h
    stringtools.h
)

include(FetchContent)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)

FetchContent_MakeAvailable(json)

FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG        v3.2.1
)

FetchContent_MakeAvailable(cxxopts)


# Executable.
add_executable(elogen ${SOURCES} ${HEADERS})

# MySQL connector libraries.
target_link_libraries(elogen PUBLIC mysqlcppconn nlohmann_json::nlohmann_json)

# Required for include paths.
target_include_directories(elogen
  PUBLIC
    ${mysqlcppconn_SOURCE_DIR}/include
    ${cxxopts_SOURCE_DIR}/include
)
