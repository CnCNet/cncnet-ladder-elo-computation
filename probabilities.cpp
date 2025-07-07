
#include <exception>

#include "probabilities.h"
#include "knownplayers.h"
#include "logging.h"
#include "rating.h"

bool Probabilities::_initialized = false;
std::array<double, 10000> Probabilities::_eloDifference;

/*!
 */
void Probabilities::initialize()
{
    if (Probabilities::_initialized)
    {
        throw std::runtime_error("Probabilities have already been initialized.");
    }

    Rating rating;

    double currentRating = 0.0;
    while (currentRating <= 3000.0)
    {
        Rating myRating(currentRating, glicko::initialDeviation, glicko::initialVolatility);
        double winningProbability = myRating.e_star(rating.toArray(), 0.0);

        winningProbability *= 10000;
        winningProbability += 0.5;

        Probabilities::_eloDifference[static_cast<int>(winningProbability)] = currentRating - glicko::initialRating;

        currentRating += 0.01;
    }

    Probabilities::_initialized = true;
}

/*!
 */
uint32_t Probabilities::wins() const
{
    return _wins;
}

/*!
 */
uint32_t Probabilities::losses() const
{
    return count() - wins();
}

/*!
 */
uint32_t Probabilities::count() const
{
    return static_cast<uint32_t>(_winningProbabilities.size());
}

/*!
 */
void Probabilities::addGame(double winningProbability, std::chrono::sys_days date, bool isWin)
{
    if (_isFinalized)
    {
        throw std::runtime_error("Trying to add probability to a finalized class.");
    }

    if (!Probabilities::_initialized)
    {
        Probabilities::initialize();
    }

    _winningProbabilities.push_back(winningProbability);
    _dates.push_back(date);
    _win.push_back(isWin);

    if (isWin)
    {
        _wins++;
    }
}

/*!
 */
ProbResult Probabilities::result(std::chrono::sys_days date) const
{
    ProbResult result{0, 0, 0.0, 0.0, 0.0};

    if (_winningProbabilities.empty())
    {
        return result;
    }

    for (size_t i = 0; i < _winningProbabilities.size(); i++)
    {
        if (_dates[i] > date)
            break;
        result.expected += _winningProbabilities[i];
        result.games++;
        result.wins += _win[i] ? 1 : 0;
        result.lastGame = std::chrono::year_month_day{ _dates[i] };
    }

    result.expected /= static_cast<double>(result.games);
    result.actual = static_cast<double>(result.wins) / static_cast<double>(result.games);

    double expEloDiffSum = Probabilities::_eloDifference[static_cast<uint32_t>(result.expected * 10000 + 0.5)];
    double curEloDiffSum = Probabilities::_eloDifference[static_cast<uint32_t>(result.actual * 10000 + 0.5)];

    Rating myRating(glicko::initialRating, glicko::initialDeviation, glicko::initialVolatility);
    double eloDiffAcc = curEloDiffSum - expEloDiffSum;

    if (result.games == result.wins)
    {
        result.normalized = 1.0;
    }
    else if (result.wins == 0)
    {
        result.normalized = 0.0;
    }
    else
    {
        result.normalized = myRating.e_star(myRating.toArray(), eloDiffAcc);
    }

    return result;
}

/*!
 */
void Probabilities::finalize()
{
    _isFinalized = true;

    if (_winningProbabilities.empty())
    {
        return;
    }

    for (double winningProbability : _winningProbabilities)
    {
        _expected += winningProbability;
    }

    _expected /= static_cast<double>(_winningProbabilities.size());

    uint32_t games = count();
    _actual = static_cast<double>(_wins) / static_cast<double>(games);

    double expEloDiffSum = Probabilities::_eloDifference[static_cast<uint32_t>(_expected * 10000 + 0.5)];
    double curEloDiffSum = Probabilities::_eloDifference[static_cast<uint32_t>(_actual * 10000 + 0.5)];

    Rating myRating(glicko::initialRating, glicko::initialDeviation, glicko::initialVolatility);
    double eloDiffAcc = curEloDiffSum - expEloDiffSum;

    if (games == _wins)
    {
        _normalized = 1.0;
    }
    else if (_wins == 0)
    {
        _normalized = 0.0;
    }
    else
    {
        _normalized = myRating.e_star(myRating.toArray(), eloDiffAcc);
    }
}

/*!
 */
bool Probabilities::isFinalized() const
{
    return _isFinalized;
}

/*!
 */
double Probabilities::expected() const
{
    if (!_isFinalized)
    {
        throw std::runtime_error("Trying to get expected win rate from an non-finalized class.");
    }

    return _expected;
}

/*!
 */
double Probabilities::actual() const
{
    if (!_isFinalized)
    {
        throw std::runtime_error("Trying to get actual win rate from an non-finalized class.");
    }

    return _actual;
}

/*!
 */
double Probabilities::result() const
{
    if (!_isFinalized)
    {
        throw std::runtime_error("Trying to get probability from an non-finalized class.");
    }

    return _normalized;
}

/*!
 */
double Probabilities::eloDifference() const
{
    if (!_isFinalized)
    {
        throw std::runtime_error("Trying to get elo diff from an non-finalized class.");
    }

    return -400.0 * std::log10((1.0 / _normalized) - 1.0);
}

bool operator<(std::pair<std::string, Probabilities> a, std::pair<std::string, Probabilities> b)
{
    return std::pair<uint32_t, Probabilities>{ 0U, a.second } < std::pair<uint32_t, Probabilities>{ 0U, b.second };
}

bool operator<(std::pair<uint32_t, Probabilities> a, std::pair<uint32_t, Probabilities> b)
{
    if (b.second.result() != a.second.result())
    {
        return b.second.result() < a.second.result();
    }
    else if (b.second.wins() != a.second.wins())
    {
        return b.second.wins() < a.second.wins();
    }
    else
    {
        return &a < &b;
    }
}

bool operator<(std::pair<std::string, const Probabilities&> a, std::pair<std::string, const Probabilities&> b)
{
    return std::pair<uint32_t, const Probabilities&>{ 0U, a.second } < std::pair<uint32_t, const Probabilities&>{ 0U, b.second };
}

bool operator<(std::pair<uint32_t, const Probabilities&> a, std::pair<uint32_t, const Probabilities&> b)
{
    if (b.second.result() != a.second.result())
    {
        return b.second.result() < a.second.result();
    }
    else if (b.second.wins() != a.second.wins())
    {
        return b.second.wins() < a.second.wins();
    }
    else
    {
        return &a < &b;
    }
}
