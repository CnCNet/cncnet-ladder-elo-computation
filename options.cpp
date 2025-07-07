
#include <iostream>

#include <cxxopts.hpp>

#include "logging.h"
#include "options.h"

/*!
 */
Options::Options(int argc, char* argv[])
{
    cxxopts::Options options("elogen", "ELO Generator for CnCNet ladder games");

    options.add_options()
        ("h,help", "Show help.")
        ("d,dry-run", "Run without writing any results (test mode).")
        ("s,statistics", "Export additional player and map statistics.")
        ("a,all-games", "Include todays games.")
        ("l,log-level", "Set the log level (debug, verbose, info, warning, error, critical, fatal).",
         cxxopts::value<std::string>()->default_value("verbose"))
        ("m,gamemode", "Set the game mode. Every available ladder abbreviation is valid.",
         cxxopts::value<std::string>())
        ("o,output-dir", "Output directory for generated JSON files.",
         cxxopts::value<std::string>())
        ("H,host", "Host name for sql connection. Overrides environment variable MYSQL_HOST. If both not set, localhost is used.",
         cxxopts::value<std::string>())
        ("P,port", "Port for sql connection. Overrides environment variable MYSQL_PORT. If both not set, 3307 is used.",
         cxxopts::value<uint32_t>())
        ("p,password", "Password for sql connection. Overrides environment variable MYSQL_PASSWORD.",
         cxxopts::value<std::string>())
        ("u,user", "User name for sql connection. Overrides environment variable MYSQL_USER.",
         cxxopts::value<std::string>())
        ("t,tournament-games", "Add tournament games from this file.",
         cxxopts::value<std::string>());

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

    if (result.count("tournament-games"))
    {
        tournamentFile = result["tournament-games"].as<std::string>();
        if (!std::filesystem::exists(tournamentFile))
        {
            std::cerr << "The file '" << tournamentFile << "' does not exist." << std::endl;
            setQuitWithErrorCode(1);
            return;
        }
    }

    gameMode = gamemodes::Unknown;

    if (!result.count("gamemode"))
    {
        std::cout << "Missing game mode. Use option --gamemode to specify. Fully supported game "
                     "modes are blitz, ra2, yr, and blitz-2v2, but others might work, too." << std::endl;
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

    if (result.count("port") > 0)
    {
        port = result["port"].as<uint32_t>();
    }

    if (result.count("host") > 0)
    {
        host = result["host"].as<std::string>();
    }

    if (result.count("user") > 0)
    {
        username = result["user"].as<std::string>();
    }

    if (result.count("password") > 0)
    {
        password = result["password"].as<std::string>();
    }

    dryRun = result["dry-run"].as<bool>();
    allGames = result["all-games"].as<bool>();
    exportFullStats = result["statistics"].as<bool>();

    if (mySqlUser().empty())
    {
        std::cerr << "No MySql user. Either use --user or set MYSQL_USER." << std::endl;
        setQuitWithErrorCode(1);
        return;
    }

    if (mySqlPassword().empty())
    {
        std::cerr << "No MySql password. Either use --password or set MYSQL_PASSWORD." << std::endl;
        setQuitWithErrorCode(1);
        return;
    }
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

/*!
 */
std::string Options::mySqlPassword() const
{
    if (!password.empty())
    {
        return password;
    }
    else
    {
        const char *envPassword = std::getenv("MYSQL_PASSWORD");
        return (envPassword != nullptr) ? std::string(envPassword) : "";
    }
}

/*!
 */
std::string Options::mySqlUser() const
{
    if (!username.empty())
    {
        return username;
    }
    else
    {
        const char *envUser = std::getenv("MYSQL_USER");
        return (envUser != nullptr) ? std::string(envUser) : "";
    }
}

/*!
 */
std::string Options::mySqlHost() const
{
    if (!host.empty())
    {
        return host;
    }
    else
    {
        const char *envHost = std::getenv("MYSQL_HOST");
        return (envHost != nullptr) ? std::string(envHost) : "localhost";
    }
}

/*!
 */
uint32_t Options::mySqlPort() const
{
    if (port != std::numeric_limits<uint32_t>::max())
    {
        return port;
    }
    else
    {
        const char *envPort = std::getenv("MYSQL_PORT");
        try
        {
            return (envPort != nullptr) ? static_cast<uint32_t>(std::stoul(std::string(envPort)))
                                        : 3307;
        }
        catch (const std::exception&)
        {
            return 3307;
        }
    }
}
