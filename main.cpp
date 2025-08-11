
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
    Log::info() << "Running elogen V" << PROJECT_VERSION << ".";
    Log::info() << "End date is " << options.endDate << ".";
    Log::info() << "Starting ELO computation.";

    Players players;
    std::map<uint32_t, Game> games = connection.fetchGames();

    // Collect all user ids involved in games.
    std::map<uint32_t, uint32_t> temporaryUserIds;
    std::set<uint32_t> finalUserIds;

    for (auto it = games.begin(); it != games.end(); ++it)
    {
        for (uint32_t i = 0; i < it->second.playerCount(); i++)
        {
            temporaryUserIds[it->second.userId(i)]++;
            if (it->second.userId(i) == 0)
            {
                Log::error() << "Invalid user id in game " << it->second.id() << ".";
            }
        }
    }

    // Now create the mapping for each user id to its primary account.
    std::unordered_map<uint32_t, uint32_t> duplicateToPrimary;

    if (options.cncnetDuplicates)
    {
        duplicateToPrimary = connection.cncnetDuplicateMapping(temporaryUserIds);
        for (const auto &[duplicate, primary] : duplicateToPrimary)
        {
            Log::verbose() << "#" << duplicate << " has primary #" << primary << ".";
        }
    }
    else if (options.noDuplicates)
    {
        Log::warning() << "Duplicates will be ignored. The resulting ranks are meant to testing purposes only.";
        for (const auto& [userId, games] : temporaryUserIds)
        {
            duplicateToPrimary[userId] = userId;
        }
    }
    else
    {
        duplicateToPrimary = connection.duplicateToPrimaryMapping(temporaryUserIds);
    }

    // Not reset all user ids in the games to primary accounts.
    for (auto it = games.begin(); it != games.end(); ++it)
    {
        for (uint32_t i = 0; i < it->second.playerCount(); i++)
        {
            if (!duplicateToPrimary.contains(it->second.userId(i)))
            {
                Log::error() << "Missing user id " << it->second.userId(i) << ".";
            }
            uint32_t primary = duplicateToPrimary[it->second.userId(i)];
            finalUserIds.insert(primary);
            it->second.setPlayer(i, primary);
        }
    }

    // Finally, load all users.
    connection.loadUsers(finalUserIds, players);

    // Run 1: Go through the games and set the correct user id for every game.
    //        Consider duplicates and only use the base account to have one
    //        elo value for each player, no matter how many account he has.
    for (auto it = games.begin(); false && it != games.end(); ++it)
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

                if (options.noDuplicates)
                {
                    userId = connection.loadPlayer(playerName, players, game.ladderAbbreviation());;
                }
                else if (options.cncnetDuplicates)
                {
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
                else
                {
                    // Use duplicates based on primary user id.
                    uint32_t temporaryUserId = game.userId(i);
                    if (!connection.loadPlayerWithNoUser(temporaryUserId, players, game.ladderAbbreviation()))
                    {
                        Log::error() << "Unable to load user with id " << temporaryUserId << ".";
                        continue;
                    }

                    Player &player = players[temporaryUserId];
                    Log::verbose() << "User " << temporaryUserId << " has primary user id " << player.primaryUserId();

                    if (player.primaryUserId() != 0 && player.primaryUserId() != temporaryUserId)
                    {
                        if (!connection.loadPlayerWithNoUser(player.primaryUserId(), players, game.ladderAbbreviation()))
                        {
                            Log::error() << "Unable to load primary user  with id " << player.primaryUserId() << ".";
                            continue;
                        }

                        Log::info() << "Resolved duplicate #" << temporaryUserId << " to #" << player.primaryUserId()
                                    << (player.hasAlias() ? player.alias() : "") << ".";

                        players.markDuplicates(player.primaryUserId(), {temporaryUserId});
                        userId = player.primaryUserId();
                    }
                    else
                    {
                        userId = temporaryUserId;
                    }
                }

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
    uint32_t skippedTestGames = 0;

    for (auto it = games.begin(); it != games.end(); ++it)
    {
        Game &game = it->second;
        game.determineWinner();

        Log::debug() << "Processing game " << game << " (Run 2).";

        uint32_t duration = game.duration();
        gametypes::GameType type = game.gameType();
        uint32_t fps = game.fps();
        std::string mapName = game.mapName();

        if (game.playerCount() != gamemodes::playerCount(options.gameMode))
        {
            Log::verbose() << "Skipping game " << game.id() << " due to player count mismatch.";
            continue;
        }

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

        // Do not consider bot for other game modes than Blitz.
        if (game.isBot() && options.gameMode != gamemodes::Blitz)
            continue;

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
                skippedTestGames++;
                continue;
            }

            players[game.userId(j)].increasePlayerNameUsage(game.playerName(j));
        }

        validGames.push_back(&game);
    }

    // Some information about skipped games.
    Log::info() << "Skipped " << skippedByFPS << " games due to low fps.";
    Log::info() << "Skipped " << skippedByDuration << " games due to duration.";
    Log::info() << "Skipped " << skippedInvalid << " games due to unknown errors.";
    Log::info() << "Skipped " << skippedTestGames << " games from test players.";

    // Sort the games by game end.
    std::sort(validGames.begin(), validGames.end(), [](const Game *a, const Game *b) {
        return (a->timestamp() + a->duration()) < (b->timestamp() + b->duration());
    });

    // Run 3 (compule elo):

    MapStats stats(options.gameMode);
    std::chrono::time_point<std::chrono::system_clock, std::chrono::days> previousGameDate{};
    std::chrono::time_point<std::chrono::system_clock, std::chrono::days> uninitializedDate{};
    Game *lastProcessedGame = nullptr;

    for (Game *game : validGames)
    {
        Log::debug() << "UNIX timestamp of game " << game->id() << " is " << game->timestamp() << ".";

        std::chrono::milliseconds timestamp = std::chrono::milliseconds(static_cast<uint64_t>(game->timestamp() + game->duration()) * 1000);
        std::chrono::system_clock::time_point currentTimePoint = std::chrono::system_clock::time_point(timestamp);
        std::chrono::system_clock::time_point shiftedTime = currentTimePoint + std::chrono::hours(options.timeShiftInHours);
        std::chrono::time_point<std::chrono::system_clock, std::chrono::days> gameDate = floor<std::chrono::days>(shiftedTime);

        Log::debug() << "Shifted time of game " << game->id() << " from "
                       << stringtools::fromDateTime(currentTimePoint) << " to " << stringtools::fromDateTime(shiftedTime) << ".";

        if (gameDate >= options.endDate)
        {
            break;
        }

        for (uint32_t j = 0; j < game->playerCount(); j++)
        {
            uint32_t id = game->userId(j);
            factions::Faction faction = game->faction(j);
            game->setRatingAndDeviation(j, players[id].elo(faction), players[id].deviation(faction));
        }

        Log::verbose() << "Processing game " << *game << " (Run 3).";

        // Date switch. Update players ELO values. If there are no valid games, no update is made and deviation won't
        // increase. It is more likely that technical reason prevented playing rather than no player showed up for
        // an entire day.
        if (previousGameDate != uninitializedDate && gameDate != previousGameDate)
        {
            Log::info() << "Apply update for " << stringtools::fromDate(previousGameDate);
            players.update();
            // In contrast to the local ELO list, the result are for the current day, which means that
            // your peak rating is set to the day where you achieved it and not they day after, when it's visible
            // for the first time.
            players.apply(previousGameDate, true, options.gameMode);
            previousGameDate = gameDate;
        }

        for (uint32_t j = 0; j < game->playerCount(); j++)
        {
            players[game->userId(j)].processGame(*game, j, false, players);
        }

        // Update map stats.
        stats.processGame(*game, players);
        lastProcessedGame = game;
        previousGameDate = gameDate;

    } // for (Game *game : validGames)

    // Process the last day.
    if (players.hasPendingGames())
    {
        Log::info() << "Apply update for " << stringtools::fromDate(previousGameDate);
        players.update();
        players.apply(previousGameDate, true, options.gameMode);
    }

    players.finalize();

    Log::info(lastProcessedGame != nullptr) << "Last game processed: " << *lastProcessedGame;

    if (options.dryRun)
        return 0;

    std::map<uint32_t, uint32_t> activeRanks = players.exportActivePlayers(options.outputDirectory, options.gameMode);
    std::map<uint32_t, uint32_t> allTimeRanks = players.exportBestOfAllTime(options.outputDirectory, options.gameMode);
    players.exportMostDaysActive(options.outputDirectory, options.gameMode);
    players.exportAlphabeticalOrder(options.outputDirectory, options.gameMode);
    players.exportNewPlayers(options.outputDirectory, options.gameMode);
    players.exportAllPlayers(options.outputDirectory, options.gameMode);

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

    stats.finalize(options.outputDirectory, players, floor<std::chrono::days>(std::chrono::system_clock::now()) - std::chrono::days{1});
    stats.exportUpsets(options.outputDirectory, players);
    stats.exportLongestGames(options.outputDirectory, players);
    stats.exportBestTeams(options.outputDirectory, players);

    // Map stats and player details not suitable for 2v2 games.
    if (gamemodes::playerCount(options.gameMode) == 2 && options.exportFullStats)
    {
        stats.exportMapsPlayed(options.outputDirectory);
        players.exportPlayerDetails(options.outputDirectory, {}, games, options.ladderAbbreviation);
    }
    Log::info() << "Exported map stats.";

    // Player ratings.
    Log::info() << "Updating table `user_ratings`.";
    connection.writePlayerRatings(options.gameMode, players, activeRanks, allTimeRanks);

    Log::info() << "All done.";
    return 0;
}
