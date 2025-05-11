 
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class Probabilities
{
public:

    static void initialize();

    //! Get the number of wins.
    uint32_t wins() const;

    //! Get the number of losses.
    uint32_t losses() const;

    //! Get the number of total games played.
    uint32_t count() const;

    //! Add a win with the given winning probability.
    void addGame(double winningProbability, bool isWin);

    //! Is this calculation finalized?
    bool isFinalized() const;

    //! Finalize the calculation.
    void finalize();

    //! Get the expected win rate.
    double expected() const;

    //! Get the actual win rate.
    double actual() const;

    //! Get the actual result.
    double result() const;

private:
    //! Winning probabilities for each game. Will be e
    //! valuated during finalization.
    std::vector<double> _winningProbabilities;

    //! Number of wins.
    uint32_t _wins = 0;

    //! Expected winning percentage.
    double _expected = 0.0;

    //! Actual winnning percentage.
    double _actual;

    //! Normalized winning probability is the winning probability
    //! against an even opponent.
    double _normalized;

    //! Not finalized by default, obviously.
    bool _isFinalized = false;

    //! Elo differences not initialized by default.
    static bool _initialized;

    //! Elo difference for winning probabily between 0.0000% to 0.9999%.
    static std::array<double, 10000> _eloDifference;
};

extern bool operator<(std::pair<uint32_t, Probabilities> a, std::pair<uint32_t, Probabilities> b);
extern bool operator<(std::pair<uint32_t, const Probabilities&> a, std::pair<uint32_t, const Probabilities&> b);
extern bool operator<(std::pair<std::string, Probabilities> a, std::pair<std::string, Probabilities> b);
extern bool operator<(std::pair<std::string, const Probabilities&> a, std::pair<std::string, const Probabilities&> b);
