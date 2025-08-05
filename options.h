
#pragma once

#include <filesystem>

#include "gamemode.h"

struct Options
{
    Options(int argc, char* argv[]);
    bool quit() const;
    int returnValue() const;

private:
    void setQuitWithErrorCode(int returnValue);

public:
    //! Get MySql host. Tries to take env var MYSQL_HOST if parameter for host was not set.
    std::string mySqlHost() const;

    //! Get MySql user name. Tries to take env var MYSQL_USER if parameter for user was not set.
    std::string mySqlUser() const;

    //! Get MySql password. Tries to take env var MYSQL_PASSWORD if parameter for password was not set.
    std::string mySqlPassword() const;

    //! Get MySql port. Tries to take env var MYSQL_PORT if parameter for port was not set.
    uint32_t mySqlPort() const;

public:
    gamemodes::GameMode gameMode;
    std::string ladderAbbreviation;
    std::filesystem::path outputDirectory;
    std::filesystem::path tournamentFile;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::days> endDate;
    int timeShiftInHours;
    bool dryRun;
    bool exportFullStats;
    bool allGames;
    bool cncnetDuplicates;
    bool noDuplicates;

private:
    std::string host;
    std::string password;
    std::string username;
    uint32_t port = std::numeric_limits<uint32_t>::max();
    int _returnValue = 0;
    bool _quit = false;

}; // class Options
