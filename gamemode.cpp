
#include "cplusplus.h"
#include "gamemode.h"
#include "rating.h"

namespace gamemodes
{

std::vector<std::string> __names = { "RA2 Blitz", "Yuris Revenge", "Red Alert 2", "Red Alert", "Red Alert 2 New Maps", "Blitz 2v2", "Red Alert 2 2v2", "Unknown" };

// These names need to match column 'abbreviation' from table 'ladders'.
std::vector<std::string> __shortNames = { "blitz", "yr", "ra2", "ra", "ra2-new-maps", "blitz-2v2", "ra2-2v2", "?" };

/*!
 */
std::string name(GameMode gameMode)
{
    return __names[std::min<uint32_t>(gameMode, count())];
}

/*!
 */
std::string shortName(GameMode gameMode)
{
    return __shortNames[std::min<uint32_t>(gameMode, count())];
}

/*!
 */
int toIndex(const std::string &name)
{
    auto it = std::find(__names.begin(), __names.end(), name);

    if (it != __names.end())
    {
        return static_cast<int>(std::distance(__names.begin(), it));
    }
    else
    {
        auto it2 = std::find(__shortNames.begin(), __shortNames.end(), name);
        return (it2!= __shortNames.end()) ? static_cast<int>(std::distance(__shortNames.begin(), it2)) : -1;
    }
}

/*!
 */
std::vector<GameMode> list()
{
    std::vector<GameMode> result;

    for (int i = 0; i < count(); i++)
    {
        result.push_back(toGameMode(i));
    }

    return result;
}

/*!
 */
GameMode toGameMode(const std::string &name)
{
    int index = toIndex(name);

    if (index < 0 || index > count())
    {
        return gamemodes::Unknown;
    }


    return static_cast<GameMode>(index);
}

/*!
 */
uint32_t playerCount(GameMode gameMode)
{
    switch (gameMode)
    {
    case Blitz:
    case YurisRevenge:
    case RedAlert2:
    case RedAlert:
    case RedAlert2NewMaps:
        return 2;
    case Blitz2v2:
    case RedAlert2_2v2:
        return 4;
    case Unknown:
        return 0;
    default:
        return 0;
    }
}

/*!
 */
double decayFactor(GameMode gameMode)
{
    if (gameMode == GameMode::YurisRevenge)
    {
        return 2.5;
    }
    else
    {
        return 3.5;
    }
}

/*!
 */
double maxDeviationAfterActive(GameMode gameMode)
{
    if (gameMode == GameMode::YurisRevenge)
    {
        return 150.0;
    }
    else
    {
        return 175.0;
    }
}

/*!
 */
double deviationThresholdActive(GameMode gameMode, double currentElo)
{
    dts::unused(gameMode);
    return std::min(75.0, 65.0 + sqrt(abs(glicko::initialRating - currentElo)));
}

/*!
 */
double deviationThresholdPeak(GameMode gameMode)
{
    return deviationThresholdActive(gameMode, glicko::initialRating) * 1.0;
}

/*!
 */
uint32_t minGamesSinceActivationForPeak(GameMode gameMode)
{
    if (gameMode == GameMode::RedAlert2_2v2 || gameMode == GameMode::Blitz2v2)
    {
        return 80;
    }
    else
    {
        return 50;
    }
}

/*!
 */
double deviationThresholdInactive(GameMode gameMode, double currentElo)
{
    if (gameMode == GameMode::YurisRevenge)
    {
        return 85.0 + log(abs(glicko::initialRating - currentElo));
    }
    else
    {
        return 85.0 + sqrt(abs(glicko::initialRating - currentElo));
    }
}

} // namespace gamemodes

