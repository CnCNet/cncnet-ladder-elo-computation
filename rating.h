
#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <ostream>
#include <vector>

#include "faction.h"

namespace glicko
{

enum Index : int
{
    Rating = 0,
    Deviation = 1,
    Volatility = 2
};

//! The default/initial rating value. 1500 works best, just like
//! suggested by the paper.
static const double initialRating = 1500.0;

//! The default/initial deviation value. Again, the suggested value
//! of 350 works best.
static const double initialDeviation = 350.0;

//! The default/initial volatility value. We stick to what the
//! paper suggests.
static const double initialVolatility = 0.06;

//! Scale factor for conversion between Glicko-1 and Glicko-2.
static const double scaleFactor = 173.7178;

//! The system constant (tau)
static const double kSystemConst = 0.3;

//! The convergence constant aka epsilon.
static const double convergence = 0.000001;

//! Glicko-2 paper states, what reasonable choices are between 0.3 and 1.2 and the system
//! should be tested to decide which value give the best accuracy in terms of prediction.
static const double tau = 0.5;

//! This is an extension to Glicko-2 to make it work with team games. The exponent factor
//! is used to determine a players share of a win/loss in a 2v2 game. This
//! value has been set after evaluation thousands of games.
static const double exponentFactor2v2 = 1.11;
}

//! Rating based on the Glicko2 rating system.
class Rating
{
public:
    //! Create a rating with the initial values.
    Rating();

    Rating(double rating, double deviation, double volatility, bool glicko2Values = false);

    enum class CalculationType
    {
        Initial = 0,
        SingleStep = 1,
        Special = 2,
        Normal = 3,
        AutoSelect = 4,
        None = 5
    };

public:
    //! Get the current rating.
    double rating() const;

    //! Get the current deviation.
    double deviation() const;

    //! Get the current volatility.
    double volatility() const;

    //! Decay function. Needs to be called after each rating period. Either resets
    //! the number of games played or increases deviation.
    void decay();

    //! Get the elo. Automatically performs the transformation of glicko2 to elo;
    double elo() const;

    //! Pending glicko2 converted to elo.
    double pendingElo() const;

    //! Current deviation converted to elo.
    double eloDeviation() const;

    //! Outputs the rating in Glicko-1 format.
    friend inline std::ostream& operator<<(std::ostream& os, const Rating& rating)
    {
        os << "[" << rating.rating() << ":" << rating.deviation() << "]";
        return os;
    }

    void apply();
    double e(const std::array<double, 3> &rating) const;
    double e_star(const std::array<double, 3> &opponent, double eloAddition) const;

private:
    //! Computation of weight factor g.
    double g(double deviation) const;

    //! Find a decent initial rating for a very weak player. Glicko-2 seems to have some
    //! issues in this case.
    std::optional<double> findInitialRatingImproved(const std::vector<std::array<double, 3>>& opponents, const std::vector<double>& results);

public:
    double variance(const std::vector<std::array<double, 3>>& opponents);
    double delta(const std::vector<std::array<double, 3>>& opponents, const std::vector<double> &results, double variance);
    double volatility(const std::vector<std::array<double, 3>>& opponents, const std::vector<double> &results, double variance);

    void updateWithNoWin(const std::vector<std::array<double, 3>>& opponents, const std::vector<double>& results, bool extendedLogging);
    void updateWithFirstWin(const std::vector<std::array<double, 3>>& opponents, const std::vector<double>& results, bool useBest, bool extendedLogging);
    void updateNormally(const std::vector<std::array<double, 3>>& opponents, const std::vector<double>& results);
    CalculationType update(const std::vector<std::array<double, 3>>& opponents, const std::vector<double>& results, CalculationType calculationType = CalculationType::AutoSelect);

    bool hasWinsAndLossesInResults(const std::vector<double> &results) const;

    CalculationType currentCalculationType() const { return _calculationType; }

    //! Get rating, deviation and volatility as an array of values.
    std::array<double, 3> toArray() const;
    std::array<double, 3> toEloArray() const;

    //! Mu, the actual rating.
    double _rating;

    //! Phi, the rating deviation.
    double _deviation;

    //! Sigma, the rating volatility.
    double _volatility;

    //! The pending rating.
    double _pendingRating;

    //! The pending deviation.
    double _pendingDeviation;

    //! The pending volatility.
    double _pendingVolatility;

    //! Total number of games played.
    uint32_t _games = 0;

    //! Number of pending games.
    uint32_t _pendingGames = 0;

    //! How do we actually calculate the rating of this player?
    CalculationType _calculationType = CalculationType::Initial;

}; // class Rating
