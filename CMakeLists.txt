
cmake_minimum_required(VERSION 3.14)
project(MySQLUserLookup LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
        message(FATAL_ERROR "GCC version must be at least 12. Detected version: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
endif()

# Requires
# sudo apt install libmysqlcppconn-dev

include(FetchContent)

set(SOURCES
    blitzmap.cpp
    main.cpp
    game.cpp
    gametype.cpp
    faction.cpp
    knownplayers.cpp
    logging.cpp
    #mapstats.cpp
    #player.cpp
    #players.cpp
    probabilities.cpp
    rating.cpp
    stringtools.cpp
)

set(HEADERS
    blitzmap.h
    cplusplus.h
    game.h
    gametype.h
    faction.h
    knownplayers.h
    logging.h
    #mapstats.h
    #player.h
    #players.h
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

# Executable
add_executable(elogen ${SOURCES} ${HEADERS})

# Link connector libraries
target_link_libraries(elogen PUBLIC mysqlcppconn nlohmann_json::nlohmann_json)

# Required for include paths
target_include_directories(elogen
  PUBLIC
    ${mysqlcppconn_SOURCE_DIR}/include
)
