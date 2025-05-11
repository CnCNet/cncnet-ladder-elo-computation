 

// C++ includes:
#include <cmath>
#include <set>

// Local includes:
#include "player.h"
//#include "database.h"
//#include "gameresult.h"
//#include "global.h"
#include "knownplayers.h"
#include "logging.h"

// TODO:
// - Best day
// - Worst day

/*!
 */
Player::Player(uint32_t userId) :
    _userId(userId)
{
    // Assigned some starting values to a couple of players. These values WILL NOT
    // change the final result in terms of gaps, but just adjust the rating of
    // everyone. If changes to the elo systems are made, we want players to keep
    // their rating, because it might look strange if the rating changes without
    // having played. Several changes have been made over time and these values
    // will just keep the ratings stable.

    if (userId == 0)
    {
        Log::warning() << "Player with user id 0 is not supposed to exist.";
    }

    std::pair<double, double> initialValues = initialRatingAndDeviation(userId);

    // Init ratings.
    for (size_t i = 0; i < _ratings.size(); i++)
    {
        _ratings[i] = Rating(initialValues.first, initialValues.second, glicko::initialVolatility);
    }

    // Init peak ratings.
    for (size_t i = 0; i < _peakRatings.size(); i++)
    {
        _peakRatings[i] = { QDateTime::fromMSecsSinceEpoch(0).toLocalTime().date(), -1.0, -1.0, static_cast<factions::Faction>(i) };
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
    if (_userId == 0xFFFFFFFF)
    {
        // Default constructor was called (probably by some container) but left
        // uninitialized.
        throw std::runtime_error("Asking for the user id of an uninitialized user.");
    }
    return _userId;
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
void Player::setAlias(const std::string &alias, bool confirmed)
{
    if (alias != "[]")
    {
        _alias = alias;
        _confirmedAlias = confirmed;
        Log::info() << "Alias for " << userId() << " is " << _alias;
        if (!confirmed)
        {
            _manuallyAssignedAlias = true;
        }
    }
    else
    {
        Log::warning() << "Rejecting bad alias '" << alias << "' for " << _account << " (" << _userId << ").";
    }
}

/*!
 */
QString Player::alias(bool considerMostOftenUsedQuickMatchName) const
{
    if ((!considerMostOftenUsedQuickMatchName || _manuallyAssignedAlias || _confirmedAlias) && !_alias.isEmpty())
    {
        return _alias;
    }
    else
    {
        if (_usedQmNames.isEmpty())
        {
            Log::warning() << "Asking for alias without having any qm names yet for player '" << this->account() << "'.";
        }
        return "[" + qmName() + "]";
    }
}

/*!
 */
std::vector<std::string> Player::aliasList() const
{
    std::vector<std::string> result = _altAliasList;

    if (_confirmedAlias || _manuallyAssignedAlias)
    {
        result.push_back(_alias);
    }

    return result;
}

/*!
 */
void Player::addAltAlias(const std::string &altAlias)
{
    _altAliasList.push_back(altAlias);
}

/*!
 */
bool Player::hasAlias(const std::string &alias) const
{
    return _altAliasList.contains(alias) || (_alias == alias);
}

/*!
 */
void Player::setCreationDate(const std::string &date)
{
    _created = date;
}

/*!
 */
std::string Player::creationDate() const
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
factions::Faction Player::getBestActiveFaction() const
{
    factions::Faction bestFaction = factions::Combined;
    double maxRating = -1.0;

    for (size_t i = 0; i < _factionStatusList.size(); i++)
    {
        factions::Faction currentFaction = factions::toFaction(i);
        if (this->isActive(currentFaction) && this->elo(currentFaction) > maxRating)
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
    if ((_statusList.size() % 2) == 0)
    {
        return -1;
    }

    if (sinceFirstActivation)
    {
        return _statusList.first().daysTo(QDate::currentDate());
    }
    else
    {
        return _statusList.last().daysTo(QDate::currentDate());
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
    return !_factionStatusList[faction].isEmpty();
}

/*!
 */
bool Player::wasActiveBefore(const std::chrono::year_month_day &date, factions::Faction faction) const
{
    if (_factionStatusList[faction].isEmpty())
    {
        return false;
    }
    else
    {
        return _factionStatusList[faction].first() < date;
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
void Player::decay()
{
    for (size_t i = 0; i < _ratings.size(); i++)
    {
        _ratings[i].decay();
    }
}

/*!
 */
Rating::CalculationType Player::update()
{
    uint32_t gameCount = 0;
    double opponentElo = 0.0;
    double result = 0.0;
    Rating::CalculationType calculationType = Rating::CalculationType::None;

    for (int i = 0; i < factions::count(); i++)
    {
        // Let's create some stats.
        if (i != factions::Combined)
            for (size_t j = 0; j < _pendingGames[i].size(); j++)
            {
                gameCount++;
                opponentElo += glicko::scaleFactor * std::get<glicko::Rating>(_pendingGames[i][j]) + glicko::initialRating;
                result += _pendingResults[i][j];
            }

        _ratings[i]._isSilent = true;

        if (!_pendingGames[i].empty())
        {
            Rating::CalculationType currentCalculationType = _ratings[i].currentCalculationType();
            if (currentCalculationType == Rating::CalculationType::Initial && !_ratings[i]._isSilent)
            {
                Log::info() << "Trying to find initial rating for player " << this->alias(true) << " and faction " << factions::name(factions::toFaction(i)) << ".";
                if (_ratings[i].hasWinsAndLossesInResults(_pendingResults[i]))
                {
                    Log::info() << "Player has wins and losses. Using the best starting value of regular and custom calculation.";
                }
                else
                {
                    Log::info() << "Player has probably losses only. Using single game calculation.";
                }
            }
            else if (currentCalculationType == Rating::CalculationType::SingleStep && !_ratings[i]._isSilent)
            {
                Log::info() << "Player " << this->alias(true) << " and faction " << factions::name(factions::toFaction(i)) << " still use single step calculation.";
            }

            calculationType = _ratings[i].update(_pendingGames[i], _pendingResults[i]);

            if (currentCalculationType == Rating::CalculationType::Initial || currentCalculationType == Rating::CalculationType::SingleStep)
            {
                if (!_ratings[i]._isSilent)
                {
                    Log::info() << "Applied improved initial elo calculation to player " << this->alias(true) << ".";
                }
            }

            if (currentCalculationType != Rating::CalculationType::Normal && _ratings[i].currentCalculationType() == Rating::CalculationType::Normal)
            {
                Log::info() << "Initial rating for player " << this->alias(true) << " and faction " << factions::name(factions::toFaction(i)) << " is [" << _ratings[i].pendingElo() << ", " << _ratings[i].deviation() << "].";
            }
            _updated[i] = true;
        }

        _pendingGames[i].clear();
        _pendingResults[i].clear();
    }

    return calculationType;
}

/*!
 */
void Player::apply(std::chrono::year_month_day date, bool decay)
{
    for (size_t i = 0; i < _ratings.size(); i++)
    {
        factions::Faction faction = factions::toFaction(i);

        if (_updated[i])
        {
            _ratings[i].apply();
        }
        else if (decay)
        {
            _ratings[i].decay();
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
        double lowerLimit = deviation(faction) - std::sqrt(std::abs(glicko::initialRating - maxElo));
        double upperLimit = deviation(faction) - std::sqrt(std::abs(glicko::initialRating - elo(faction)));
        double ll = 65.0;
        double uu = 85.0;

        gamemodes::GameMode gameMode = Database::instance().gameMode();
        if (gameMode == gamemodes::YurisRevenge)
        {
            lowerLimit = deviation(faction) - std::sqrt(std::abs(glicko::initialRating - maxElo)); // = deviation(faction);
            upperLimit = deviation(faction) - std::sqrt(std::abs(glicko::initialRating - elo(faction))); //deviation(faction);
            ll = 65.0; //75.0;
            uu = 85.0; //95.0;
        }


        if (lowerLimit < ll && !isActive(faction))
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
            Log::debug() << date.toString() << " => " << alias(true)<< " goes active after " << _gameCount[i] << " games for faction '" << factions::name(faction) << "'.";
        }
        else if (upperLimit > uu && isActive(faction))
        {
            // Too bad... going inactive.
            Log::debug() << date.toString() << " => " << alias(true) << " goes inactive for faction '" << factions::name(faction) << "'.";

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

        _eloByDate[date][i] = std::pair<double, double>(result, deviation);
    }
}

/*!
 */
int Player::daysActive() const
{
    int result = 0;

    for (auto it = _eloByDate.keyValueBegin(); it != _eloByDate.keyValueEnd(); ++it)
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
    return _lastGame.daysTo(QDate::currentDate());
}

/*!
 */
int Player::daysFromFirstGame() const
{
    return _firstGame.daysTo(QDate::currentDate());
}

/*!
 */
int Player::daysToInactivity() const
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
                testRating.decay();
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

    for (auto it = _eloByDate.keyValueBegin(); it != _eloByDate.keyValueEnd(); ++it)
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

    if (game.gameType() == gametypes::Quickmatch)
    {
        addQmNameUsage(game.playerName(index));
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
        ss << "Player with index " << index << " in game " << game.id() << " is " << game.player(index) << ", but expected was "
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
        opponent = game.player(index ^ 1);
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
        double myElo = players[game.playerId(playerIndex)].rating(faction).elo();
        uint32_t mateIndex = game.mateIndex(playerIndex);
        double matesElo = players[game.playerId(mateIndex)].rating(game.faction(mateIndex)).elo();
        double matesDeviation = players[game.playerId(mateIndex)].rating(game.faction(mateIndex)).eloDeviation();

        //double myShare = myElo / (myElo + matesElo);
        double myShare = std::log(myElo) / (std::log(myElo) + std::log(matesElo));

        if (!game.hasWon(playerIndex))
        {
            myShare = 1.0 - myShare;
        }

        double rating_diff = std::abs(myElo - matesElo);
        double scalingMyTeam = 1.0 + (rating_diff / 500.0);

        std::pair<uint32_t, uint32_t> opponents = game.opponentsIndex(playerIndex);
        double opponent1Elo = players[game.player(opponents.first)].rating(game.faction(opponents.first)).elo();
        double opponent2Elo = players[game.player(opponents.second)].rating(game.faction(opponents.second)).elo();
        double opponent1Deviation = players[game.player(opponents.first)].rating(game.faction(opponents.first)).eloDeviation();
        double opponent2Deviation = players[game.player(opponents.second)].rating(game.faction(opponents.second)).eloDeviation();

        //std::cout << "Opponents elo: " << opponent1Elo << " " << opponent2Elo << std::endl;

        double finalElo = (opponent1Elo + opponent2Elo) * myShare;
        double finalDeviation = (opponent1Deviation + opponent2Deviation + matesDeviation) / 3.0 * scalingMyTeam;

        std::array<double, 3> opponent{((finalElo - glicko::initialRating) / glicko::scaleFactor ),
                                       (finalDeviation / glicko::scaleFactor),
                                       glicko::initialVolatility};
        //std::cout << myShare << " Opponent: [" << finalElo << ", " << finalDeviation << "]" << std::endl;

        double diffx = myElo + matesElo - opponent1Elo - opponent2Elo;
        static int wins = 0;
        static int losses = 0;
        static double diffsum = 0;
        if (_userId == 3118 && myElo - 300 > matesElo && finalDeviation < 80.0 && diffx > 0)
        {
            diffsum += diffx;
            if (result > 0.5)
                wins++;
            else
                losses++;
            std::cout << game << std::endl;
            std::cout << wins << ":" << losses << " (" << (diffsum / (wins + losses)) << ") = " << ((double)wins / ((double)wins + (double)losses)) << "%" << std::endl;
        }

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

        /* OLD COMPUTE
        static int won = 0;
        static int loss = 0;

        double scalingMyTeam = 1.0;
        double scalingOtherTeam = 1.0;

        double myElo = players[game.player(playerIndex)].rating(faction).elo();
        uint32_t mateIndex = game.mateIndex(playerIndex);
        double matesElo = players[game.player(mateIndex)].rating(game.faction(mateIndex)).elo();
        double otherDeviation = players[game.player(mateIndex)].rating(game.faction(mateIndex)).eloDeviation();

        if (!game.hasWon(playerIndex) && myElo > matesElo)
        {
            double rating_diff = std::abs(myElo - matesElo);
            scalingMyTeam += (rating_diff / 250.0);
        }

        //double deviationPenalty = scaling * players[game.player(mateIndex)].rating(game.faction(mateIndex)).eloDeviation();

        std::pair<uint32_t, uint32_t> opponents = game.opponentsIndex(playerIndex);
        double opponent1Elo = players[game.player(opponents.first)].rating(game.faction(opponents.first)).elo();
        double opponent2Elo = players[game.player(opponents.second)].rating(game.faction(opponents.second)).elo();
        double ratingDiffOpponents = std::abs(opponent1Elo - opponent2Elo);

        if (!game.hasWon(playerIndex) && myElo > matesElo)
        {
            scalingOtherTeam += (ratingDiffOpponents / 500.0);
        }

        //if (deviationPenalty > 2.0)
        std::cout << game.playerName(index).toStdString() << " SCALE " << (scalingMyTeam * scalingOtherTeam) << " in " << game << std::endl;

        //std::cout << alias(true).toStdString() << "[" << _ratings[factions::Combined].elo() << ", " << _ratings[factions::Combined].deviation() << "]" << std::endl;

        for (uint32_t i = 0; i < game.playerCount(); i++)
        {
            if (game.hasWon(playerIndex) != game.hasWon(i))
            {
                // This is an opponent.
                opponent = game.player(i);
                opponentFaction = game.faction(i);

                if (!players.contains(opponent))
                {
                    std::stringstream ss;
                    ss << "Unable to find opponent " << opponent << " while processing game.";
                    throw std::runtime_error(ss.str());
                }

                std::array<double, 3> rating = players[opponent].rating(opponentFaction).toArray();
                double dev = rating[1] * glicko::scaleFactor;
                std::cout << "Old deviation: " << dev << std::endl;
                if (!game.hasWon(playerIndex))
                    dev = std::sqrt(((dev * dev) + (otherDeviation * otherDeviation)) * 0.5);
                dev = dev * scalingMyTeam * scalingOtherTeam;
                dev = std::min(350.0, dev);
                rating[1] = dev / glicko::scaleFactor;
                std::cout << "New deviation: " << rating[1] * glicko::scaleFactor << std::endl;

                if (instantProcessing)
                {
                    _ratings[faction].update({ rating }, { result });
                    _updated[faction] = true;
                    _ratings[factions::Combined].update({ rating }, { result });
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
        }*/

        //_ratings[faction].apply();
        //_ratings[factions::Combined].apply();
        //std::cout << alias(true).toStdString() << "[" << _ratings[factions::Combined].elo() << ", " << _ratings[factions::Combined].deviation() << "]" << std::endl;

        /*std::vector<std::array<double, 3>> opponentRatings;

        for (uint32_t i = 0; i < game.playerCount(); i++)
        {
            if (game.hasWon(playerIndex) != game.hasWon(i))
            {
                opponentRatings.push_back(players[game.player(i)].rating(game.faction(i)).toArray());
            }
        }

        double opponentAverageRating = 0.0;
        double opponentRDsum = 0.0;

        for (const auto& r : opponentRatings)
        {
            opponentAverageRating += r[0];
            opponentRDsum += r[1] * r[1];
        }
        opponentAverageRating /= opponentRatings.size();
        double opponentCombinedRD = std::sqrt(opponentRDsum / opponentRatings.size());

        std::vector<std::array<double, 3>> ownTeamRatings;

        for (uint32_t i = 0; i < game.playerCount(); i++)
        {
            if (i != playerIndex && (game.hasWon(playerIndex) == game.hasWon(i)))
            {
                ownTeamRatings.push_back(players[game.player(i)].rating(game.faction(i)).toArray());
            }
        }

        double ownTeammateRating = 0.0;
        double ownTeammateRD = 0.0;

        if (!ownTeamRatings.empty())
        {
            ownTeammateRating = ownTeamRatings[0][0];
            ownTeammateRD = ownTeamRatings[0][1];
        }

        double ownShare = _ratings[faction].toArray().at(0) / (_ratings[faction].toArray().at(0) + ownTeammateRating);

        std::cout << "OWN: [" << ownTeammateRating << ", " << ownTeammateRD << "] ";
        std::cout << " VS: [" << opponentAverageRating << ", " << opponentCombinedRD << "]" << std::endl;

        Rating ownRating(ownTeammateRating, ownTeammateRD, glicko::initialVolatility, true);
        Rating otherRating(opponentAverageRating, opponentCombinedRD, glicko::initialVolatility, true);

        double expectedScore = ownRating.e_star(otherRating.toArray(), 0.0);

        double adjustedScore = std::min(1.0, ownShare / expectedScore);
        std::cout << "SCORE " << adjustedScore << " for " << game.playerName(playerIndex).toStdString() << " (expected " << expectedScore << ") => " << game << std::endl;

        if (!game.hasWon(playerIndex))
        {
            adjustedScore = 0.0; //1.0 - std::min(1.0, ownShare / (1.0 - expectedScore));
        }

        if (instantProcessing)
        {
            _ratings[faction].update({ { opponentAverageRating, opponentCombinedRD, glicko::initialVolatility } }, { adjustedScore });
            _updated[faction] = true;
        }
        else
        {
            _pendingGames[faction].push_back({ opponentAverageRating, opponentCombinedRD, glicko::initialVolatility });
            _pendingResults[faction].push_back(adjustedScore);
        }*/
    }

    _gameCount[faction]++;
    _gameCount[factions::Combined]++;

    // Remember when the last game was played.
    _lastGame = game.date();

    if (!_firstGame.isValid())
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
        Probabilities &probs = _vsPlayer[game.player(index ^ 1)];
        Rating myRating(game.rating(index), game.deviation(index), glicko::initialVolatility);
        Rating otherRating(game.rating(index ^ 1), game.deviation(index ^ 1), glicko::initialVolatility);
        double expectedWinningPercentage = myRating.e_star(otherRating.toArray(), 0.0);
        probs.addGame(expectedWinningPercentage, (game.winnerIndex() == index));

        // Map statistics.
        int mapIndex = map::toIndex(game.mapName());
        if (mapIndex >= 0)
        {
            gamesetup::Setup setup = gamesetup::fromFactions(game.faction(index), game.faction(index ^ 1));
            Probabilities &probs = _mapStats[setup][mapIndex];
            double expectedWinRate = myRating.e_star(otherRating.toArray(), 0.0);
            probs.addGame(expectedWinRate, game.winnerIndex() == index);
        }
    }
}

/*!
 */
bool Player::hasConfirmedAlias() const
{
    return _confirmedAlias;
}

/*!
 */
const std::vector<std::string> &Player::qmNames(gamemodes::GameMode gameMode) const
{
    return _qmNames[gameMode];
}

/*!
 */
void Player::addQmName(gamemodes::GameMode gameMode, const std::string &name, bool mightExist)
{
    if (gameMode == gamemodes::Unknown)
    {
        Log::error() << "Cannot add quick match name to unknown game mode.";
        return;
    }

    if (!_qmNames[gameMode].contains(name))
    {
        _qmNames[gameMode].push_back(name);
    }
    else if (!mightExist)
    {
        Log::warning() << "Quickmatch name '" << name << "' already exists for user " << _userId << ".";
    }
}

/*!
 */
void Player::addQmNameUsage(const std::string &name)
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
std::string Player::qmName() const
{
    std::string qmName;
    uint32_t appearances = 0;

    for (std::map<std::string, uint32_t>::const_iterator it = _usedQmNames.begin(); it != _usedQmNames.end(); ++it)
    {
        if (it.value() > appearances)
        {
            qmName = it.key();
            appearances = it.value();
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

    for (size_t i = 0; i < gamesetup::Unknown; i++)
    {
        for (int j = 0; j < map::count(); j++)
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
const Probabilities& Player::mapStats(gamesetup::Setup setup, int mapIndex) const
{
    if (static_cast<size_t>(setup) >= _mapStats.size() || mapIndex < 0 || static_cast<size_t>(mapIndex) >= _mapStats[setup].size())
    {
        throw std::out_of_range("Indices for map stats are out of range.");
    }

    return _mapStats[setup][mapIndex];
}

/*!
 */
std::map<std::chrono::year_month_day, std::pair<double, double>> Player::historicalElo(factions::Faction faction) const
{
    std::map<std::chrono::year_month_day, std::pair<double, double>> result;

    for (auto it = _eloByDate.keyValueBegin(); it != _eloByDate.keyValueEnd(); ++it)
    {
        const std::chrono::year_month_day &date = it->first;
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
    std::string me = this->alias(true);
    std::string him = other.alias(true);

    if (me.isEmpty() && him.isEmpty())
        return false;

    if (me.isEmpty())
        return true;

    if (him.isEmpty())
        return false;

    while (me[0] == "[")
        me.remove(0, 1);

    while (him[0] == "[")
        him.remove(0, 1);

    return me.toLower() < him.toLower();
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

