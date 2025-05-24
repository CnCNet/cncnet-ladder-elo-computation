
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
    gamemodes::GameMode gameMode;
    std::string ladderAbbreviation;
    std::filesystem::path outputDirectory;
    std::string host;
    uint32_t port;
    bool dryRun;
    bool exportFullStats;

private:
    int _returnValue = 0;
    bool _quit = false;
};
