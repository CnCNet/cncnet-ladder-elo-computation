 
#include <cmath>
#include <set>

#include "faction.h"
#include "knownplayers.h"
#include "logging.h"
#include "player.h"
#include "players.h"
#include "stringtools.h"

// TODO:
// - Best day
// - Worst day

/*!
 */
Player::Player() :
    _userId(0)
{

}


/*!
 */
Player::Player(uint32_t userId, uint32_t primaryUserId, const std::string &username, gamemodes::GameMode gameMode) :
    _userId(userId),
    _primaryUserId(primaryUserId),
    _account(username)
{


    if (userId == 0)
    {
        Log::warning() << "Player with user id 0 is not supposed to exist.";
    }

    std::pair<double, double> initialValues = players::initialRatingAndDeviation(userId, gameMode);

    // Init ratings.
    for (size_t i = 0; i < _ratings.size(); i++)
    {
        _ratings[i] = Rating(initialValues.first, initialValues.second, glicko::initialVolatility);
    }

    // Init peak ratings.
    for (size_t i = 0; i < _peakRatings.size(); i++)
    {
        static_assert(!std::chrono::year_month_day{std::chrono::year{0}, std::chrono::month{2}, std::chrono::day{31}}.ok());
        _peakRatings[i] = { {std::chrono::year{0}, std::chrono::month{2}, std::chrono::day{31}}, -1.0, -1.0, static_cast<factions::Faction>(i) };
    }
}

/*!
 */
Player::~Player()
{
    // Nothing to do.
}

/*!
 */
uint32_t Player::userId() const
{
    if (_userId == 0)
    {
        // Default constructor was called (probably by some container) but left
        // uninitialized.
        throw std::runtime_error("Asking for the user id of an uninitialized user.");
    }
    return _userId;
}

/*!
 */
uint32_t Player::primaryUserId() const
{
    return _primaryUserId;
}

/*!
 */
void Player::setAccount(const std::string &account)
{
    _account = account;
}

/*!
 */
std::string Player::account() const
{
    return _account;
}

/*!
 */
void Player::setAlias(const std::string &alias)
{
    if (alias != "[]")
    {
        _alias = alias;
        Log::info() << "Alias for " << userId() << " is " << _alias;
    }
    else
    {
        Log::error() << "Rejecting bad alias '" << alias << "' for " << _account << " (" << _userId << ").";
    }
}

/*!
 */
bool Player::hasAlias() const
{
    return !_alias.empty();
}

/*!
 */
std::string Player::alias() const
{
    if (!_alias.empty())
    {
        return _alias;
    }

    if (_usedQmNames.empty())
    {
        Log::warning() << "Asking for alias without having any player names yet for player '" << this->account() << "'.";
        return "???";
    }

    return "[" + mostOftenUsedPlayerName() + "]";
}

/*!
 */
void Player::setCreationDate(const std::chrono::year_month_day &date)
{
    _created = date;
}

/*!
 */
std::chrono::year_month_day Player::creationDate() const
{
    return _created;
}

/*!
 */
double Player::deviation(factions::Faction faction) const
{
    return _ratings[faction].deviation() * glicko::scaleFactor;
}

/*!
 */
double Player::elo(factions::Faction faction) const
{
    return _ratings[faction].rating() * glicko::scaleFactor + glicko::initialRating;
}

/*!
 */
double Player::yesterdaysElo(factions::Faction faction) const
{
    return _yesterdaysRatings[faction].rating() * glicko::scaleFactor + glicko::initialRating;
}

/*!
 */
uint32_t Player::yesterdaysGameCount() const
{
    return _yesterdaysRatings[factions::Combined].gameCount();
}

/*!
 */
bool Player::isActive(factions::Faction faction) const
{
    // Because entries are alternatining dates when players goes active/inactive,
    // the faction status list just needs to contain an odd number in order
    // for the faction to be active.
    return (_factionStatusList[faction].size() & 1) == 1;
}

/*!
 */
bool Player::isActive() const
{
    for (int i = 0; i < factions::count(); i++)
    {
        if (isActive(factions::toFaction(i)))
        {
            return true;
        }
    }

    return false;
}

/*!
 */
double Player::maxRating(bool includeInactive) const
{
    double result = -1.0;

    for (size_t i = 0; i < _factionStatusList.size(); i++)
    {
        if (this->isActive(factions::toFaction(i)))
        {
            result = std::max(result, this->elo(factions::toFaction(i)));
        }
        else if (includeInactive && this->wasActive(factions::toFaction(i)))
        {
            result = std::max(result, this->elo(factions::toFaction(i)));
        }
    }

    return result;
}

/*!
 */
double Player::yesterdaysMaxRating(bool includeInactive) const
{
    double result = -1.0;

    for (size_t i = 0; i < _factionStatusList.size(); i++)
    {
        if (this->isActive(factions::toFaction(i)))
        {
            result = std::max(result, this->yesterdaysElo(factions::toFaction(i)));
        }
        else if (includeInactive && this->wasActive(factions::toFaction(i)))
        {
            result = std::max(result, this->yesterdaysElo(factions::toFaction(i)));
        }
    }

    return result;
}

/*!
 */
factions::Faction Player::getBestFaction(bool includeInactive) const
{
    factions::Faction bestFaction = factions::Combined;
    double maxRating = -1.0;

    for (size_t i = 0; i < _factionStatusList.size(); i++)
    {
        factions::Faction currentFaction = factions::toFaction(i);
        if ((this->isActive(currentFaction) || includeInactive) && this->elo(currentFaction) > maxRating)
        {
            bestFaction = currentFaction;
            maxRating = this->elo(currentFaction);
        }
    }

    return bestFaction;
}

/*!
 */
int Player::daysActive(bool sinceFirstActivation) const
{
    auto today = floor<std::chrono::days>(std::chrono::system_clock::now());
    std::chrono::year_month_day currentDate = std::chrono::year_month_day{today};
    std::chrono::sys_days currentDays = currentDate;

    if ((_statusList.size() % 2) == 0)
    {
        return -1;
    }

    if (sinceFirstActivation)
    {
        std::chrono::sys_days days = _statusList.front();
        std::chrono::days diff = currentDays - days;
        return diff.count();
    }
    else
    {
        std::chrono::sys_days days = _statusList.back();
        std::chrono::days diff = currentDays - days;
        return diff.count();
    }
}

/*!
 */
bool Player::wasActive() const
{
    return !_statusList.empty();
}

/*!
 */
bool Player::wasActive(factions::Faction faction) const
{
    // Because entries are alternatining dates when players goes active/inactive,
    // the faction status list just needs to contain an odd number in order
    // for the faction to be active.
    return !_factionStatusList[faction].empty();
}

/*!
 */
bool Player::wasActiveBefore(const std::chrono::year_month_day &date, factions::Faction faction) const
{
    if (_factionStatusList[faction].empty())
    {
        return false;
    }
    else
    {
        return _factionStatusList[faction].front() < date;
    }
}


/*!
 */
double Player::volatility(factions::Faction faction) const
{
    return _ratings[faction].volatility();
}

/*!
 */
void Player::update()
{
    for (int i = 0; i < factions::count(); i++)
    {
        _updated[i] = !_pendingGames[i].empty();

        if (!_pendingGames[i].empty())
        {
            Rating::CalculationType currentCalculationType = _ratings[i].currentCalculationType();
            if (currentCalculationType == Rating::CalculationType::Initial)
            {
                Log::info() << "Trying to find initial rating for player " << this->alias() << " and faction " << factions::name(factions::toFaction(i)) << ".";
                if (_ratings[i].hasWinsAndLossesInResults(_pendingResults[i]))
                {
                    Log::info() << "Player has wins and losses. Using the best starting value of regular and custom calculation.";
                }
                else
                {
                    Log::info() << "Player has probably losses only. Using single game calculation.";
                }
            }
            else if (currentCalculationType == Rating::CalculationType::SingleStep)
            {
                Log::info() << "Player " << this->alias() << " and faction " << factions::name(factions::toFaction(i)) << " still use single step calculation.";
            }

            Rating::CalculationType appliedCalculationType = _ratings[i].update(_pendingGames[i], _pendingResults[i]);

            if (currentCalculationType == Rating::CalculationType::Initial || currentCalculationType == Rating::CalculationType::SingleStep)
            {
                Log::info() << "Applied improved initial ELO calculation to player " << this->alias() << ".";
            }

            if (appliedCalculationType != Rating::CalculationType::Normal && _ratings[i].currentCalculationType() == Rating::CalculationType::Normal)
            {
                Log::info() << "Initial rating for player " << this->alias() << " and faction " << factions::name(factions::toFaction(i)) << " is [" << _ratings[i].pendingElo() << ", " << _ratings[i].deviation() << "].";
            }
        }

        _pendingGames[i].clear();
        _pendingResults[i].clear();
    }
}

/*!
 */
void Player::apply(std::chrono::year_month_day date, bool decay, gamemodes::GameMode gameMode)
{
     _yesterdaysRatings = _ratings;

    for (size_t i = 0; i < _ratings.size(); i++)
    {
        factions::Faction faction = factions::toFaction(i);

        if (_updated[i])
        {
            _ratings[i].apply();
        }
        else if (decay)
        {
            _ratings[i].decay(this->wasActive(), gamemodes::decayFactor(gameMode), gamemodes::maxDeviationAfterActive(gameMode));
        }

        _updated[i] = false;

        double maxElo = elo(faction);

        if (faction == factions::Combined)
        {
            // Use the highest rating.
            for (size_t j = 0; j < _ratings.size(); j++)
            {
                if (factions::toFaction(j) != factions::Combined && elo(factions::toFaction(j)) > maxElo)
                {
                    maxElo = elo(factions::toFaction(j));
                }
            }
        }

        if (deviation(faction) < gamemodes::deviationThresholdActive(gameMode, elo(faction)) && !isActive(faction))
        {
            if (!isActive())
            {
                if (_statusList.empty())
                {
                    _initialRating = elo(faction);
                    _gamesToBecomeActive = this->gameCount();
                }

                _statusList.push_back(date);
            }

            // Congratulations! This faction has just gone active.
            _factionStatusList[i].push_back(date);
            Log::debug() << date << " => " << alias()<< " goes active after " << _gameCount[i] << " games for faction '" << factions::name(faction) << "'.";
        }
        else if (deviation(faction) > gamemodes::deviationThresholdInactive(gameMode, elo(faction)) && isActive(faction))
        {
            // Too bad... going inactive.
            Log::debug() << date << " => " << alias() << " goes inactive for faction '" << factions::name(faction) << "'.";

            // Going full inactive.
            _factionStatusList[i].push_back(date);

            if (!isActive())
            {
                _statusList.push_back(date);
            }
        }

        if (isActive(faction) && (elo(faction) - this->deviation(faction)) > _peakRatings[faction].adjustedElo)
        {
            // New peak rating.
            _peakRatings[faction].date = date;
            _peakRatings[faction].adjustedElo = elo(faction) - this->deviation(faction);
            _peakRatings[faction].deviation = this->deviation(faction);
        }

    }

    for (size_t i = 0; i < factions::count(); i++)
    {
        double result = -1.0;
        double deviation = 0.0;

        if (this->isActive(factions::toFaction(i)))
        {
            if (this->elo(factions::toFaction(i)) > result)
            {
                result = this->elo(factions::toFaction(i));
                deviation = this->deviation(factions::toFaction(i));
            }
        }

        auto y = static_cast<int>(date.year());
        auto m = static_cast<unsigned>(static_cast<unsigned>(date.month()));
        auto d = static_cast<unsigned>(static_cast<unsigned>(date.day()));
        uint32_t intDate = static_cast<uint32_t>(y) * 10000 + m * 100 + d;

        _eloByDate[intDate][i] = std::pair<double, double>(result, deviation);
    }
}

/*!
 */
int Player::daysActive() const
{
    int result = 0;

    for (auto it = _eloByDate.begin(); it != _eloByDate.end(); ++it)
    {
        const std::array<std::pair<double, double>, factions::count()> &eloByDate = it->second;
        for (size_t i = 0; i < eloByDate.size(); i++)
        {
            if (eloByDate[i].first > 0.0)
            {
                result++;
                break;
            }
        }
    }

    return result;
}

/*!
 */
int Player::daysFromLastGame() const
{
    auto today = floor<std::chrono::days>(std::chrono::system_clock::now());
    std::chrono::year_month_day currentDate = std::chrono::year_month_day{today};
    std::chrono::sys_days currentDays = currentDate;

    std::chrono::sys_days days = _lastGame;
    std::chrono::days diff = currentDays - days;
    return diff.count();
}

/*!
 */
int Player::daysFromFirstGame() const
{
    auto today = floor<std::chrono::days>(std::chrono::system_clock::now());
    std::chrono::year_month_day currentDate = std::chrono::year_month_day{today};
    std::chrono::sys_days currentDays = currentDate;

    std::chrono::sys_days days = _firstGame;
    std::chrono::days diff = currentDays - days;
    return diff.count();
}

/*!
 */
int Player::daysToInactivity(gamemodes::GameMode gameMode) const
{
    int days = 0;

    for (int i = 0; i < factions::count(); i++)
    {
        int currentDays = 0;

        factions::Faction faction = factions::toFaction(i);

        if (this->isActive(faction))
        {
            Rating testRating = _ratings[faction];
            while (currentDays < 1000 && (testRating.deviation() * glicko::scaleFactor) - std::sqrt(std::abs(glicko::initialRating - elo(faction))) < 85.0)
            {
                currentDays++;
                testRating.decay(true, gamemodes::decayFactor(gameMode), gamemodes::maxDeviationAfterActive(gameMode));
            }

            days = std::max(currentDays, days);
        }
    }

    return days;
}

/*!
 */
int Player::daysInactive() const
{
    int result = 0;

    for (auto it = _eloByDate.begin(); it != _eloByDate.end(); ++it)
    {
        const std::array<std::pair<double, double>, factions::count()> &eloByDate = it->second;
        bool foundActivity = false;

        // Now iterate over faction.
        for (size_t i = 0; i < eloByDate.size() && !foundActivity; i++)
        {
            if (eloByDate[i].first > 0.0)
            {
                result = 0;
                // Activity for a single faction set inactive days to 0.
                foundActivity = true;
            }
        }

        if (!foundActivity)
        {
            result++;
        }

    }
    return result;
}
/*!
 */
void Player::processGame(const Game& game, int index, bool instantProcessing, const Players &players)
{
    factions::Faction faction = game.faction(index);
    factions::Faction opponentFaction;
    uint32_t opponent;
    double result;

    if (_userId == 0)
    {
        throw std::runtime_error("Processing game for uninitialized player with userId 0.");
    }

    if (faction >= factions::UnknownFaction)
    {
        std::stringstream ss;
        ss << "Unknown faction " << static_cast<int>(faction) << " for player " << _userId << " in game " << game.id() << ".";
        throw std::runtime_error(ss.str());
    }

    if (game.gameType() == gametypes::Quickmatch)
    {
        increasePlayerNameUsage(game.playerName(index));
    }

    int playerIndex = game.playerIndex(_userId);

    if (playerIndex < 0)
    {
        std::stringstream ss;
        ss << "Unknown player " << playerIndex << " while processing game " << game.id() << ".";
        Log::error() << ss.str();
        throw std::runtime_error(ss.str());
    }
    else if (playerIndex != index)
    {
        std::stringstream ss;
        ss << "Player with index " << index << " in game " << game.id() << " is " << game.userId(index) << ", but expected was "
           << _userId << ". Skipped this game for player " << _userId << ".";
        throw std::runtime_error(ss.str());
    }

    if (game.isDraw())
    {
        _draws++;
        result = 0.5;
    }
    else if (game.hasWon(playerIndex))
    {
        _wins++;
        result = 1.0;
    }
    else if (!game.hasWon(playerIndex))
    {
        _losses++;
        result = 0.0;
    }

    if (game.playerCount() == 2)
    {
        opponent = game.userId(index ^ 1);
        opponentFaction = game.faction(index ^ 1);

        if (!players.contains(opponent))
        {
            std::stringstream ss;
            ss << "Unable to find opponent " << opponent << " while processing game.";
            throw std::runtime_error(ss.str());
        }

        std::array<double, 3> rating = players[opponent].rating(opponentFaction).toArray();

        //instantProcessing = true;
        if (instantProcessing)
        {
            _ratings[faction].update({ rating }, { result }, Rating::CalculationType::Normal);
            _updated[faction] = true;

            _ratings[factions::Combined].update({ rating }, { result }, Rating::CalculationType::Normal);
            _updated[factions::Combined] = true;
        }
        else
        {
            _pendingGames[faction].push_back(rating);
            _pendingResults[faction].push_back(result);

            _pendingGames[factions::Combined].push_back(rating);
            _pendingResults[factions::Combined].push_back(result);
        }
    }
    else
    {
        // ELO for 2v2 games.
        double myElo = players[game.userId(playerIndex)].rating(faction).elo();
        uint32_t mateIndex = game.mateIndex(playerIndex);
        double mateElo = players[game.userId(mateIndex)].rating(game.faction(mateIndex)).elo();
        double mateDeviation = players[game.userId(mateIndex)].rating(game.faction(mateIndex)).eloDeviation();

        double myStrength = std::pow(myElo, glicko::exponentFactor2v2);
        double mateStrength = std::pow(mateElo, glicko::exponentFactor2v2);

        double myShare = myStrength / (myStrength + mateStrength);

        if (!game.hasWon(playerIndex))
        {
            myShare = 1.0 - myShare;
        }

        std::pair<uint32_t, uint32_t> opponents = game.opponentsIndices(playerIndex);
        double opponent1Elo = players[game.userId(opponents.first)].rating(game.faction(opponents.first)).elo();
        double opponent2Elo = players[game.userId(opponents.second)].rating(game.faction(opponents.second)).elo();
        double opponent1Deviation = players[game.userId(opponents.first)].rating(game.faction(opponents.first)).eloDeviation();
        double opponent2Deviation = players[game.userId(opponents.second)].rating(game.faction(opponents.second)).eloDeviation();

        double finalElo = (opponent1Elo + opponent2Elo) * myShare;
        double finalDeviation = (opponent1Deviation + opponent2Deviation + mateDeviation) / 3.0;

        std::array<double, 3> opponent{((finalElo - glicko::initialRating) / glicko::scaleFactor ),
                                       (finalDeviation / glicko::scaleFactor),
                                       glicko::initialVolatility};

        if (instantProcessing)
        {
            _ratings[faction].update({ opponent }, { result });
            _updated[faction] = true;
            _ratings[factions::Combined].update({ opponent }, { result });
            _updated[factions::Combined] = true;
        }
        else
        {
            _pendingGames[faction].push_back(opponent);
            _pendingResults[faction].push_back(result);

            _pendingGames[factions::Combined].push_back(opponent);
            _pendingResults[factions::Combined].push_back(result);
        }
    }

    _gameCount[faction]++;
    _gameCount[factions::Combined]++;

    // Remember when the last game was played.
    _lastGame = game.date();

    if (!_firstGame.ok())
    {
        _firstGame = game.date();
    }

    // Does this game count for highest rated victories? 2v2 games are not taken into account.
    if (game.playerCount() == 2 && game.winnerIndex() == index && game.deviation(index) < 200.0 && players[opponent].wasActive() && game.isUnderdogWin())
    {
        double diff = (game.rating(index ^ 1) - game.deviation(index ^ 1)) - (game.rating(index) + game.deviation(index));
        _hightestRatedVictories.insert({ game.id(), diff });
        if (_hightestRatedVictories.size() > 20)
        {
            _hightestRatedVictories.erase(_hightestRatedVictories.begin());
        }
    }


    // Does this game count for lowest rated defeats? 2v2 games are not taken into account.
    if (game.playerCount() == 2 && game.winnerIndex() == (index ^ 1) && game.deviation(index) < 200.0 && players[opponent].wasActive() && game.isUnderdogWin())
    {
        double diff = (game.rating(index) - game.deviation(index)) - (game.rating(index ^ 1) + game.deviation(index ^ 1));
        _lowestRatedDefeats.insert({ game.id(), diff });
        if (_lowestRatedDefeats.size() > 20)
        {
            _lowestRatedDefeats.erase(_lowestRatedDefeats.begin());
        }
    }

    if (!game.isDraw() && game.playerCount() == 2)
    {
        // Update player vs player stats and maps stats. Only applies to games
        // with 2 players.
        Probabilities &probs = _vsPlayer[game.userId(index ^ 1)];
        Rating myRating(game.rating(index), game.deviation(index), glicko::initialVolatility);
        Rating otherRating(game.rating(index ^ 1), game.deviation(index ^ 1), glicko::initialVolatility);
        double expectedWinningPercentage = myRating.e_star(otherRating.toArray(), 0.0);
        probs.addGame(expectedWinningPercentage, game.sysDate(), game.winnerIndex() == index);

        // Map statistics (currently only for blitz).
        int mapIndex = blitzmap::toIndex(game.mapName());
        if (mapIndex >= 0)
        {
            factions::Setup setup = factions::fromFactions(game.faction(index), game.faction(index ^ 1));
            Probabilities &probs = _mapStats[setup][mapIndex];
            double expectedWinRate = myRating.e_star(otherRating.toArray(), 0.0);
            probs.addGame(expectedWinRate, game.sysDate(), game.winnerIndex() == index);
        }
    }
}

/*!
 */
void Player::addName(const std::string &name, const std::string &ladder)
{
    if (_names[ladder].contains(name))
    {
        Log::warning() << "Name '" << name << "' already exists for user " << _userId << " on ladder " << ladder << ".";
    }
    else
    {
        _names[ladder].insert(name);
    }
}

/*!
 */
void Player::increasePlayerNameUsage(const std::string &name)
{
    if (!_usedQmNames.contains(name))
    {
        _usedQmNames[name] = 1;
    }
    else
    {
        _usedQmNames[name]++;
    }
}

/*!
 */
uint32_t Player::gameCount() const
{
    uint32_t result = 0;
    for (int i = 0; i < factions::count() - 1; i++)
    {
        result += _gameCount[i];
    }

    return result;
}

/*!
 */
uint32_t Player::pendingGameCount() const
{
    return static_cast<uint32_t>(_pendingGames[factions::Combined].size());
}

/*!
 */
uint32_t Player::gameCount(factions::Faction faction) const
{
    return  _gameCount[faction];
}

/*!
 */
std::string Player::mostOftenUsedPlayerName() const
{
    std::string qmName;
    uint32_t appearances = 0;

    for (std::map<std::string, uint32_t>::const_iterator it = _usedQmNames.begin(); it != _usedQmNames.end(); ++it)
    {
        if (it->second > appearances)
        {
            qmName = it->first;
            appearances = it->second;
        }
    }

    return qmName;
}

/*!
 */
PeakRating Player::peakRating(factions::Faction faction) const
{
    return _peakRatings[faction];
}

/*!
 */
PeakRating Player::peakRating() const
{
    double maxElo = _peakRatings[0].adjustedElo;
    size_t maxIndex = 0;

    for (size_t i = 0; i < factions::count(); i++)
    {
        if (_peakRatings[i].adjustedElo > maxElo)
        {
            maxElo = _peakRatings[i].adjustedElo;
            maxIndex = i;
        }
    }

    return _peakRatings[maxIndex];
}

/*!
 */
std::chrono::year_month_day Player::firstGame() const
{
    return _firstGame;
}

/*!
 */
std::chrono::year_month_day Player::lastGame() const
{
    return _lastGame;
}

/*!
 */
const std::set<HighestRatedVictories>& Player::highestRatedVictories() const
{
    return _hightestRatedVictories;
}

/*!
 */
const std::set<LowestRatedDefeats>& Player::lowestRatedDefeats() const
{
    return _lowestRatedDefeats;
}

/*!
 */
void Player::finalize()
{
    for (auto it = _vsPlayer.begin(); it != _vsPlayer.end(); ++it)
    {
        if (!it->second.isFinalized())
        {
            it->second.finalize();
        }
    }

    for (size_t i = 0; i < factions::UnknownSetup; i++)
    {
        for (int j = 0; j < blitzmap::count(); j++)
        {
            Probabilities &probs = _mapStats[i][j];
            probs.finalize();
        }
    }
}

/*!
 */
const std::map<uint32_t, Probabilities>& Player::vsOtherPlayers() const
{
    return _vsPlayer;
}

/*!
 */
const Probabilities& Player::mapStats(factions::Setup setup, int mapIndex) const
{
    if (static_cast<size_t>(setup) >= _mapStats.size() || mapIndex < 0 || static_cast<size_t>(mapIndex) >= _mapStats[setup].size())
    {
        throw std::out_of_range("Indices for map stats are out of range.");
    }

    return _mapStats[setup][mapIndex];
}

/*!
 */
std::map<uint32_t, std::pair<double, double>> Player::historicalElo(factions::Faction faction) const
{
    std::map<uint32_t, std::pair<double, double>> result;

    for (auto it = _eloByDate.begin(); it != _eloByDate.end(); ++it)
    {
        uint32_t date = it->first;
        const std::array<std::pair<double, double>, factions::count()> &ratingAndDeviation = it->second;
        if (ratingAndDeviation[faction].first > 0.0)
        {
            result[date] = ratingAndDeviation[faction];
        }
    }

    return result;
}

/*!
 */
bool Player::lowerLexicalOrder(const Player &other) const
{
    std::string me = this->alias();
    std::string him = other.alias();

    if (me.empty() && him.empty())
        return false;

    if (me.empty())
        return true;

    if (him.empty())
        return false;

    while (me[0] == '[')
        me.erase(0, 1);

    while (him[0] == '[')
        him.erase(0, 1);

    return stringtools::toLower(me) < stringtools::toLower(him);
}

/*!
 */
uint32_t Player::wins() const
{
    return _wins;
}

/*!
 */
uint32_t Player::losses() const
{
    return _losses;
}

/*!
 */
uint32_t Player::draws() const
{
    return _draws;
}

/*!
 */
const Rating& Player::rating(factions::Faction faction) const
{
    return _ratings[faction];
}

/*!
 */
const std::map<std::string, std::set<std::string>>& Player::names() const
{
    return _names;
}

