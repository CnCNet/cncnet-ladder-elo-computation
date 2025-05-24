
#include <iostream>

#include <cxxopts.hpp>

#include "logging.h"
#include "options.h"

/*!
 */
Options::Options(int argc, char* argv[])
{
    cxxopts::Options options("elogen", "Elo Generator for CnCNet ladder games");

    options.add_options()
        ("h,help", "Show help.")
        ("d,dry-run", "Run without writing any results (test mode).")
        ("s,statistics", "Export additional player and map statistics.")
        ("l,log-level", "Set the log level (debug, verbose, info, warning, error, critical, fatal).",
         cxxopts::value<std::string>()->default_value("verbose"))
        ("m,gamemode", "Set the game mode. Every available ladder abbreviation is valid.",
         cxxopts::value<std::string>())
        ("o,output-dir", "Output directory for generated JSON files.",
         cxxopts::value<std::string>())
        ("H,host", "Host name for sql connection.",
         cxxopts::value<std::string>()->default_value("localhost"))
        ("p,port", "Port for sql connection",
         cxxopts::value<uint32_t>()->default_value("3307"));

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
        std::cout << options.help() << "\n";
        setQuitWithErrorCode(0);
        return;
    }

    if (result.count("log-level"))
    {
        Log::Level level = Log::fromString(result["log-level"].as<std::string>());
        if (level == Log::NoLog)
        {
            std::cerr << "Unknown log level." << std::endl;
            setQuitWithErrorCode(1);
            return;
        }
        Log::setGlobalLogLevel(level);
    }

    outputDirectory = std::filesystem::current_path();

    if (result.count("output-dir"))
    {
        outputDirectory = result["output-dir"].as<std::string>();
        if (!std::filesystem::is_directory(outputDirectory))
        {
            std::cerr << "The directory '" << outputDirectory << "' does not exist." << std::endl;
            setQuitWithErrorCode(1);
            return;
        }
    }

    gameMode = gamemodes::Unknown;

    if (!result.count("gamemode"))
    {
        std::cout << "Missing game mode. Use option --gamemode to specify. Fully supported game "
                     "modes are blitz, ra2, yr, and blitz-2v2, but other might work, too." << std::endl;
        setQuitWithErrorCode(1);
    }
    else
    {
        ladderAbbreviation = result["gamemode"].as<std::string>();
        if (ladderAbbreviation == "ra2-new-maps")
        {
            std::cout << "Ladder 'ra2-new-maps' is usually integrated in the ra2 ladder, but computing "
                      << " elo for ra2-new-maps will work." << std::endl;
        }
        gameMode = gamemodes::toGameMode(ladderAbbreviation);
    }

    host = result["host"].as<std::string>();
    port = result["port"].as<uint32_t>();
    dryRun = result["dry-run"].as<bool>();
    exportFullStats = result["statistics"].as<bool>();
}

/*!
 */
void Options::setQuitWithErrorCode(int returnValue)
{
    _returnValue = returnValue;
    _quit = true;
}

/*!
 */
bool Options::quit() const
{
    return _quit;
}

/*!
 */
int Options::returnValue() const
{
    return _returnValue;
}
