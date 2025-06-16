 
#include <cassert>

#include "logging.h"
#include "rating.h"

/*!
 */
Rating::Rating()
{
    _rating = (glicko::initialRating - glicko::initialRating) / glicko::scaleFactor;
    _deviation = glicko::initialDeviation / glicko::scaleFactor;
    _volatility = glicko::initialVolatility;

    _pendingRating = _rating;
    _pendingDeviation = _deviation;
    _pendingVolatility = _volatility;
}

/*!
 */
Rating::Rating(double rating, double deviation, double volatility, bool glicko2Values) :
    _volatility(volatility)
{
    if (!glicko2Values)
    {
        _rating = (rating - glicko::initialRating) / glicko::scaleFactor;
        _deviation = deviation / glicko::scaleFactor;
    }
    else
    {
        _rating = rating;
        _deviation = deviation;
    }

    _pendingRating = _rating;
    _pendingDeviation = _deviation;
    _pendingVolatility = _volatility;
}


/*!
 */
double Rating::rating() const
{
    return _rating;
}

/*!
 */
double Rating::deviation() const
{
    return _deviation;
}

/*!
 */
double Rating::g(double deviation) const
{
    double scale = deviation / std::numbers::pi;
    return 1.0 / sqrt(1.0 + 3.0 * scale * scale);
}

/*!
 */
double Rating::e_star(const std::array<double, 3> &opponent, double eloAddition) const
{
    double exponent = -1.0 * (_rating  + eloAddition / glicko::scaleFactor - std::get<glicko::Rating>(opponent));
    return 1.0 / (1.0 + std::exp(exponent));
}

/*!
 */
double Rating::e(const std::array<double, 3> &opponent) const
{
    double exponent = -1.0 * g(std::get<glicko::Deviation>(opponent)) * (_rating - std::get<glicko::Rating>(opponent));
    return 1.0 / (1.0 + std::exp(exponent));
}

/*!
 */
double Rating::variance(const std::vector<std::array<double, 3>> &opponents)
{
    double variance = 0.0;

    for (size_t i = 0; i < opponents.size(); i++)
    {
        double e = this->e(opponents[i]);
        variance += std::pow(g(std::get<glicko::Deviation>(opponents[i])), 2) * e * (1.0 - e);
    }

    return 1.0 / variance;
}

/*!
 */
double Rating::delta(const std::vector<std::array<double, 3>> &opponents, const std::vector<double>& results, double variance)
{
    double delta = 0.0;

    for (size_t i = 0; i < opponents.size(); i++)
    {
        delta += g(std::get<glicko::Deviation>(opponents[i])) * (results[i] - e(opponents[i]));
    }

    return delta * variance;
}

/*!
 */
double Rating::volatility(const std::vector<std::array<double, 3>> &opponents, const std::vector<double>& results, double variance)
{
    // Step 4
    const double delta = this->delta(opponents, results, variance);

    // Step 5
    // 5.1
    const double a = log(pow(_volatility, 2));
    auto f = [&, this](double x)
    {
        return exp(x) * (pow(delta, 2) - pow(_deviation, 2) - variance - exp(x)) / (2 * pow(pow(_deviation, 2) + variance + exp(x), 2)) - (x - a) / pow(glicko::tau, 2);
    };

    // 5.2
    double A = a;
    double B, k;
    if (pow(delta, 2) > pow(_deviation, 2) + variance)
    {
        B = log(pow(delta, 2) - pow(_deviation, 2) - variance);
    }
    else
    {
        k = 1.0;
        while (f(a - k * glicko::tau) < 0)
        {
            k = k + 1;
        }
        B = a - k * glicko::tau;
    }

    // 5.3
    double fA = f(A);
    double fB = f(B);
    double C, fC;

    // 5.4

    // GLICKO-BUG: Does not converge in any case. Needed to add a counter
    //           : to use worse values.
    uint32_t steps = 0;
    double convergence = glicko::convergence;

    while (std::abs(B - A) > convergence)
    {
        C = A + (A - B) * fA / (fB - fA);
        fC = f(C);
        if (fC * fB < 0.0)
        {
            A = B;
            fA = fB;
        }
        else
        {
            fA = fA / 2.0;
        }
        B = C;
        fB = fC;
        steps++;
        if (steps > 100000)
        {
            steps = 0;
            convergence *= 10;
        }
    }

    // 5.5
    return exp(A / 2.0);
}

void Rating::updateWithNoWin(
    const std::vector<std::array<double, 3>>& opponents,
    const std::vector<double>& results,
    bool extendedLogging)
{
    assert(opponents.size() == results.size());

    Rating rating = *this;

    for (size_t i = 0; i < opponents.size(); i++)
    {
        if (extendedLogging)
        {
            Log::info() << "Game " << i + 1 << " vs [" << (std::get<glicko::Rating>(opponents[i]) * glicko::scaleFactor) + glicko::initialRating << ", " <<
                (std::get<glicko::Deviation>(opponents[i]) * glicko::scaleFactor) << ", " << std::get<glicko::Volatility>(opponents[i]) << "] =>" << results[i];
        }
    }

    for (size_t i = 0; i < opponents.size(); i++)
    {
        rating.updateNormally({opponents[i]}, {results[i]});
        rating.apply();
    }

    updateNormally(opponents, results);
    _pendingRating = rating._pendingRating;

    if (extendedLogging)
    {
        Log::info() << "First rating is " << ((_pendingRating * glicko::scaleFactor) + glicko::initialRating) << ".";
    }
}

void Rating::updateWithFirstWin(
    const std::vector<std::array<double, 3>>& opponents,
    const std::vector<double>& results,
    bool useBest,
    bool extendedLogging)
{
    assert(opponents.size() == results.size());

    if (extendedLogging)
    {
        for (size_t i = 0; i < opponents.size(); i++)
        {
            Log::info() << "Game " << i + 1 << " vs [" << (std::get<glicko::Rating>(opponents[i]) * glicko::scaleFactor) + glicko::initialRating << ", " <<
                (std::get<glicko::Deviation>(opponents[i]) * glicko::scaleFactor) << ", " << std::get<glicko::Volatility>(opponents[i]) << "] =>" << results[i];
        }
    }

    updateNormally(opponents, results);
    double currentRating = (_pendingRating * glicko::scaleFactor) + glicko::initialRating;
    if (extendedLogging)
    {
        Log::info() << "Try to find better rating than [" << currentRating << ";" << _pendingDeviation * glicko::scaleFactor << "]." << useBest;
    }

    std::optional<double> betterRating = findInitialRatingImproved(opponents, results);
    double tempRating = (betterRating.value() - glicko::initialRating) / glicko::scaleFactor;

    _pendingRating = useBest ? std::max(_pendingRating, tempRating) : tempRating;

    if (extendedLogging)
    {
        Log::info() << "Better rating than " << currentRating << " is [" << ((_pendingRating * glicko::scaleFactor) + glicko::initialRating) << ";" << _pendingDeviation * glicko::scaleFactor << "].";
    }
}

void Rating::updateNormally(const std::vector<std::array<double, 3>>& opponents, const std::vector<double>& results)
{
    double variance = this->variance(opponents);

    _pendingVolatility = this->volatility(opponents, results, variance); // Based on old deviation and vola.
    _pendingDeviation = sqrt(pow(_deviation, 2) + pow(_pendingVolatility, 2));
    _pendingDeviation = 1.0 / sqrt((1.0 / pow(_pendingDeviation, 2)) + (1 / variance));

    double ratingDeviationSum = 0.0;

    for (size_t i = 0; i < opponents.size(); i++)
    {
        ratingDeviationSum += this->g(std::get<glicko::Deviation>(opponents[i])) * (results[i] - this->e(opponents[i]));
    }

    _pendingRating += (pow(_pendingDeviation, 2) * ratingDeviationSum);

    _pendingGames++;
    _games++;
}

/*!
 */
Rating::CalculationType Rating::update(const std::vector<std::array<double, 3>>& opponents, const std::vector<double>& results, CalculationType calculationType)
{
    if (eloDeviation() < 200.0 || calculationType == CalculationType::Normal)
    {
        updateNormally(opponents, results);
        _calculationType = CalculationType::Normal;
        return CalculationType::Normal;
    }
    else if ((eloDeviation() <= 200.0 &&_calculationType == CalculationType::SingleStep && hasWinsAndLossesInResults(results)) || calculationType == CalculationType::Special)
    {
        // Moving from single calculation to special calculation.
        updateWithFirstWin(opponents, results, false, true);
        _calculationType = CalculationType::Normal;
        return CalculationType::Special;
    }
    else if (eloDeviation() > 200.0 && hasWinsAndLossesInResults(results))
    {
        // Moving from single calculation to special calculation.
        updateWithFirstWin(opponents, results, true, true);
        _calculationType = CalculationType::Special;
        return CalculationType::Special;
    }
    else if (eloDeviation() > 200.0)
    {
        updateWithNoWin(opponents, results, true);
        _calculationType = CalculationType::SingleStep;
        return CalculationType::SingleStep;
    }
    else
    {
        updateWithNoWin(opponents, results, true);
        _calculationType = CalculationType::SingleStep;
        return CalculationType::SingleStep;
    }
}

/*!
 */
void Rating::decay(bool wasActive, double factor, double maxDeviationAfterActive)
{
    if (_games == 0)
    {
        // Convert internal deviation to true deviation.
        double trueDeviation = deviation() * glicko::scaleFactor;

        // This is a custom deviation function.
        trueDeviation = std::min(wasActive ? maxDeviationAfterActive : 350.0, trueDeviation + (std::pow(std::log(trueDeviation) / std::log(factor), factor) / 100.0f));

        // Back to internal deviation.
        _deviation = trueDeviation / glicko::scaleFactor;
    }
    else
    {
        // Just reset the number of games played.
        _games = 0;
    }
}

/*!
 */
void Rating::apply()
{
    _volatility = _pendingVolatility;
    _deviation = _pendingDeviation;
    _rating = _pendingRating;
    _pendingGames = 0;
}

/*!
 */
std::array<double, 3> Rating::toArray() const
{
    return { _rating, _deviation, _volatility };
}

/*!
 */
std::array<double, 3> Rating::toEloArray() const
{
    return { elo(), eloDeviation(), _volatility };
}


/*!
 */
double Rating::volatility() const
{
    return _volatility;
}

/*!
 */
double Rating::elo() const
{
    return _rating * glicko::scaleFactor + glicko::initialRating;
}

/*!
 */
double Rating::pendingElo() const
{
    return _pendingRating * glicko::scaleFactor + glicko::initialRating;
}


/*!
 */
double Rating::eloDeviation() const
{
    return _deviation * glicko::scaleFactor;
}

/*!
 */
std::optional<double> Rating::findInitialRatingImproved(const std::vector<std::array<double, 3>>& opponents, const std::vector<double>& results)
{
    double bestDiff = std::numeric_limits<double>::max();
    double improvedElo;
    double currentElo = 3000.0;

    while (currentElo > 100.0)
    {
        Rating rating(currentElo, glicko::initialDeviation, glicko::initialVolatility);
        rating.updateWithNoWin(opponents, results, false);
        rating.apply();

        if (abs(currentElo - rating.elo()) < bestDiff)
        {
            bestDiff = abs(currentElo - rating.elo());
            improvedElo = rating.elo();
        }

        currentElo -= 100.0;
    }

    currentElo = improvedElo + 50.0;
    double destinationElo = improvedElo - 50.0;

    while (currentElo > destinationElo)
    {
        Rating rating(currentElo, glicko::initialDeviation, glicko::initialVolatility);
        rating.updateWithNoWin(opponents, results, false);
        rating.apply();

        if (abs(currentElo - rating.elo()) < bestDiff)
        {
            bestDiff = abs(currentElo - rating.elo());
            improvedElo = rating.elo();
        }

        currentElo -= 10.0;
    }

    currentElo = improvedElo + 5.0;
    destinationElo = improvedElo - 5.0;

    while (currentElo > destinationElo)
    {
        Rating rating(currentElo, glicko::initialDeviation, glicko::initialVolatility);
        rating.updateWithNoWin(opponents, results, false);
        rating.apply();

        if (abs(currentElo - rating.elo()) < bestDiff)
        {
            bestDiff = abs(currentElo - rating.elo());
            improvedElo = rating.elo();
        }

        currentElo -= 1.0;
    }

    return improvedElo;
}

/*!
 */
bool Rating::hasWinsAndLossesInResults(const std::vector<double> &results) const
{
    // No rating yet. Make sure pending games contains wins and losses.
    bool hasWins = false;
    bool hasLosses = false;
    for (size_t i = 0; i < results.size(); i++)
    {
        hasWins |= (results[i] > 0.5);
        hasLosses |= (results[i] < 0.5);
    }

    return (hasWins && hasLosses);
}
