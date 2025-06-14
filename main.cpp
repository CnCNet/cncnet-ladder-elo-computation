
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <nlohmann/json.hpp>

#include "cplusplus.h"
#include "databaseconnection.h"
#include "game.h"
#include "gameoverlay.h"
#include "logging.h"
#include "mapstats.h"
#include "options.h"
#include "players.h"
#include "stringtools.h"

int main(int argc, char* argv[])
{
    Options options(argc, argv);
    if (options.quit())
        return options.returnValue();

    DatabaseConnection connection(options);
    if (options.gameMode == gamemodes::Unknown)
    {
        if (!connection.ladderExists(options.ladderAbbreviation))
        {
            std::cerr << "Game mode '" << options.ladderAbbreviation << "' is no known abbreviation for a ladder." << std::endl;
            return 1;
        }
        else
        {
            std::cout << "Game mode '" << options.ladderAbbreviation << "' is exists, but has not dedicated "
                      << " support. Result might be poor." << std::endl;
        }
    }

    if (!connection.isEstablished())
    {
        return 1;
    }

    Log::info();
    Log::addTimestampAndLogLevel(true);
    Log::info() << "Starting ELO computation.";
    Log::info() << "Initiating duplicates.";
    connection.initDuplicates();

    // Some known duplicates, some not recognized by cncnet. That's why they are set manually.
    connection.addDuplicates(  152, { 37747, 79486 });
    connection.addDuplicates(  268, {    69 });
    connection.addDuplicates( 3968, { 18319 });
    connection.addDuplicates(17651, { 40343, 43364, 44568 });
    connection.addDuplicates(19083, { 10459 });
    connection.addDuplicates(19548, { 68698 });
    connection.addDuplicates(33933, { 300, 5878 });
    connection.addDuplicates(37077, { 58873, 59236, 59916, 68898, 68942, 71304 });
    connection.addDuplicates(40500, {    24,  1029, 68169 });
    connection.addDuplicates(44616, { 67416 });
    connection.addDuplicates(47880, { 71623 });
    connection.addDuplicates(53313, { 59298, 76620 });
    connection.addDuplicates(54423, { 20498 });
    connection.addDuplicates(55626, { 73649 }); // Also 39160, but auto-detected
    connection.addDuplicates(58766, { 58764, 66502 });
    connection.addDuplicates(59413, {   554, 61680 });
    connection.addDuplicates(60300, { 61757, 65104, 65875 });
    connection.addDuplicates(62077, { 56736 });
    connection.addDuplicates(63398, { 63331 });
    connection.addDuplicates(67132, { 1179 });
    connection.addDuplicates(67596, { 36814 });
    connection.addDuplicates(69904, { 73057, 75285, 78280});
    connection.addDuplicates(60828, { 77657, 74819});
    connection.removeDuplicate(56589);

    Log::info() << "Duplicates initiated.";

    Players players;
    std::map<uint32_t, Game> games = connection.fetchGames();

    // Run 1: Go through the games and set the correct user id for every game.
    //        Consider duplicates and only use the base account to have one
    //        elo value for each player, no matter how many account he has.
    for (auto it = games.begin(); it != games.end(); ++it)
    {
        Game &game = it->second;

        Log::debug() << "Processing game " << game.id() << " (Run 1).";

        for (uint32_t i = 0; i < game.playerCount(); i++)
        {
            std::string playerName = game.playerName(i);
            uint32_t userId = players.userId(playerName, game.ladderAbbreviation());

            if (userId == 0)
            {
                Log::debug() << "Player '" << playerName << "' is still unknown. Trying to find that player in the db.";

                // Load the player first.
                uint32_t temporaryUserId = connection.loadPlayer(playerName, players, game.ladderAbbreviation());

                // Known users are for the current ladder only, but ra2 ladder might contain yr
                // player from 02/2022 and 04/2022. It might also contains player from ra2-new-maps.
                if (!connection.knownUser(temporaryUserId) && game.ladderAbbreviation() == options.ladderAbbreviation)
                {
                    Log::error() << "Did not expect player '" << playerName << "' (" << temporaryUserId << ") to have played a game.";
                    continue;
                }

                // Now load its duplicates.
                std::set<uint32_t> duplicates = connection.getDuplicates(temporaryUserId);
                Log::verbose(!duplicates.empty()) << "Duplicates of user " << temporaryUserId << " are: " << duplicates;

                std::set<uint32_t> forbiddenBaseAccounts;

                for (uint32_t duplicate : duplicates)
                {
                    if (players.contains(duplicate))
                    {
                        Log::warning() << "User " << duplicate << " already loaded while processing duplicates. This is not supposed to happen.";
                        continue;
                    }

                    if (!connection.loadPlayer(duplicate, players, game.ladderAbbreviation()))
                    {
                        if (!connection.loadPlayerWithNoUser(duplicate, players, game.ladderAbbreviation()))
                        {
                            Log::info() << "User " << duplicate << " not part of table 'users' and thus cannot "
                                        << "be used as a base account.";
                            forbiddenBaseAccounts.insert(duplicate);
                        }
                    }
                }

                // Get the base user account. Might be the current id, that's why
                // it is inserted in the list of duplicates.
                duplicates.insert(temporaryUserId);
                userId = connection.getBaseAccount(duplicates, forbiddenBaseAccounts);
                duplicates.erase(userId);
                Log::debug(!duplicates.empty()) << "Final duplicates of user " << userId << " are: " << duplicates;

                // Consider all duplicates and link all available player names to the base account.
                players.markDuplicates(userId, duplicates);
                Log::fatal(userId != players.userId(playerName, game.ladderAbbreviation()))
                    << "Processing of duplicates went wrong (" << userId
                     << " != " << players.userId(playerName, game.ladderAbbreviation()) << ").";
            }

            game.setPlayer(i, userId);
            players[userId].increasePlayerNameUsage(playerName);
        }

        Log::debug() << "Processed " << game;
    }

    // Adding tournament games is a hack. Can't add new players afterwards and game ids are fixed.
    if (!options.tournamentFile.empty())
    {
        Log::info() << "Loading tournament games from '" << options.tournamentFile << "'.";
        GameOverlay overlay;
        overlay.loadTournamentGames(connection, options.tournamentFile, players, options.gameMode, options.ladderAbbreviation, games);
        Log::info() << "Tournament games added.";
    }

    // Run 2: Sort out certain games and create a vector of valid games for further processing.

    std::vector<Game*> validGames; // Keep a list of valid games.
    std::map<std::string, int> ignoredMaps; // Only for blitz mode.
    std::map<std::string, int> unknownPlayers;
    uint32_t skippedByDuration = 0;
    uint32_t skippedByFPS = 0;
    uint32_t skippedInvalid = 0;

    for (auto it = games.begin(); it != games.end(); ++it)
    {
        Game &game = it->second;
        game.determineWinner();

        Log::debug() << "Processing game " << game << " (Run 2).";

        uint32_t duration = game.duration();
        gametypes::GameType type = game.gameType();
        uint32_t fps = game.fps();
        std::string mapName = game.mapName();

        // Ignore games with less than 35 seconds.
        if (type == gametypes::Quickmatch && duration != 0 && duration < 35)
        {
            skippedByDuration++;
            continue;
        }

        // Ignore games with less than 40 fps.
        if (type == gametypes::Quickmatch && fps > 1 && fps < 40)
        {
            Log::verbose() << "Skipping game " << game.id() << " due to " << fps << " fps.";
            skippedByFPS++;
            continue;
        }

        // Ignore games on non-ELO maps in blitz.
        if (options.gameMode == gamemodes::Blitz && blitzmap::toIndex(mapName) == -1)
        {
            if (!ignoredMaps.contains(mapName))
            {
                Log::info() << "Ignoring blitz games on map " << mapName << ".";
                ignoredMaps[mapName] = 0;
            }
            ignoredMaps[mapName]++;
            continue;
        }

        // Ignore games with unknown errors.
        if (!game.isValid())
        {
            Log::info() << "Ignoring " << game;
            skippedInvalid++;
            continue;
        }

        // Ignore games with unknown players and test accounts.
        for (uint32_t j = 0; j < game.playerCount(); j++)
        {
            if (!players.contains(game.userId(j)))
            {
                Log::verbose() << "Game " << game.id() << " contains an unknown player " << game.playerName(j) << " (" << game.userId(j) << ").";
                unknownPlayers[game.playerName(j)] = game.userId(j);
                continue;
            }

            if (players.isTestAccount(game.userId(j)))
            {
                Log::info() << "Player '" << game.playerName(j) << "' is a test player. "
                            << "Game " << game.id() << " will be ignored.";
                continue;
            }
        }

        validGames.push_back(&game);
    }

    // Some information about skipped games.
    Log::info() << "Skipped " << skippedByFPS << " games due to low fps.";
    Log::info() << "Skipped " << skippedByDuration << " games due to duration.";
    Log::info() << "Skipped " << skippedInvalid << " games due to unknown errors.";

    // Sort the games by game end.
    std::sort(validGames.begin(), validGames.end(), [](const Game *a, const Game *b) {
        return (a->timestamp() + a->duration()) < (b->timestamp() + b->duration());
    });

    // Run 3 (compule elo):

    MapStats stats(options.gameMode);
    uint64_t timestamp = games.begin()->second.timestamp();
    std::chrono::milliseconds ms_since_epoch = std::chrono::milliseconds(timestamp * 1000) - std::chrono::hours(24); // TODO: Why subtract 1 day?
    std::chrono::system_clock::time_point time_point = std::chrono::system_clock::time_point(ms_since_epoch);
    std::chrono::time_point<std::chrono::system_clock, std::chrono::days> currentDate = floor<std::chrono::days>(time_point);
    std::chrono::time_point<std::chrono::system_clock, std::chrono::days> lastDate;

    for (Game *game : validGames)
    {
        Log::debug() << "UNIX timestamp of game " << game->id() << " is " << game->timestamp() << ".";

        std::chrono::milliseconds timestamp = std::chrono::milliseconds(static_cast<uint64_t>(game->timestamp() + game->duration()) * 1000);
        std::chrono::system_clock::time_point currentTimePoint = std::chrono::system_clock::time_point(timestamp);

        // Days will be switches at UTC+5. So EST is the time zone for flipping days. The game time will still stay
        // UTC, but for historical ELO, peak rating, etc... the ELO is taken at this specific time. For the locally
        // generated ELO list it was supposed to be UTC+1, but was UTC-1 by accident.
        std::chrono::system_clock::time_point shiftedTime = currentTimePoint - std::chrono::hours(5);
        std::chrono::time_point<std::chrono::system_clock, std::chrono::days> date = floor<std::chrono::days>(shiftedTime);

        Log::verbose() << "Shifted time of game " << game->id() << " from "
                       << stringtools::fromDateTime(currentTimePoint) << " to " << stringtools::fromDateTime(shiftedTime) << ".";

        lastDate = date;

        for (uint32_t j = 0; j < game->playerCount(); j++)
        {
            uint32_t id = game->userId(j);
            factions::Faction faction = game->faction(j);
            game->setRatingAndDeviation(j, players[id].elo(faction), players[id].deviation(faction));
        }

        Log::verbose() << "Processing game " << *game << " (Run 3).";

        // Date switch. Update players ELO values.
        if (date != currentDate && !validGames.empty())
        {
            Log::info() << "Apply update for " << stringtools::fromDate(date);
            players.update();
            // In constrast to the local ELO list, the result are for the current day, which means that
            // your peak rating is set to the day where you achieved it and not they day after, when it's visible
            // for the first time.
            players.apply(currentDate, true);
            currentDate = date;
        }

        for (uint32_t j = 0; j < game->playerCount(); j++)
        {
            players[game->userId(j)].processGame(*game, j, false, players);
        }

        // Update map stats.
        if (game->playerCount() == 2)
        {
            stats.processGame(*game, players);
        }

    } // for (Game *game : validGames)

    // Process the last day.
    players.update();
    players.apply(lastDate, true);
    players.finalize();

    if (!options.dryRun)
    {
        players.exportActivePlayers(options.outputDirectory, options.gameMode);
        players.exportBestOfAllTime(options.outputDirectory, options.gameMode);
        players.exportMostDaysActive(options.outputDirectory, options.gameMode);
        players.exportAlphabeticalOrder(options.outputDirectory, options.gameMode);
        players.exportNewPlayers(options.outputDirectory, options.gameMode);
    }

    for (auto it = ignoredMaps.begin(); it != ignoredMaps.end(); ++it)
    {
        Log::info() << "Ignored " << it->second << " games on '" << it->first << "'.";
    }
    for (auto it = unknownPlayers.begin(); it != unknownPlayers.end(); ++it)
    {
        Log::info() << "Unknown player " << it->first << " (" << it->second << ").";
    }
    if (unknownPlayers.empty())
    {
        Log::info() << "No unknown players have been found.";
    }

    Log::info() << "Processed " << validGames.size() << " games. About to finalize stats.";

    stats.finalize(options.outputDirectory, players);
    stats.exportUpsets(options.outputDirectory, players);
    stats.exportLongestGames(options.outputDirectory, players);

    // Map stats and player details not suitable for 2v2 games.
    if (gamemodes::playerCount(options.gameMode) == 2 && options.exportFullStats)
    {
        stats.exportMapsPlayed(options.outputDirectory);
        players.exportPlayerDetails(options.outputDirectory, {}, games, options.ladderAbbreviation);
    }

    return 0;
}
