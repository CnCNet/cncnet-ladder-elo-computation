
#include "gamemode.h"

namespace gamemodes
{

std::vector<std::string> __names = { "RA2 Blitz", "Yuris Revenge", "Red Alert 2", "Red Alert", "Red Alert 2 New Maps", "Blitz 2v2", "Unknown" };

// These names need to match column 'abbreviation' from table 'ladders'.
std::vector<std::string> __shortNames = { "blitz", "yr", "ra2", "ra", "ra2-new-maps", "blitz-2v2", "?" };

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
        return 4;
    case Unknown:
        return 0;
    default:
        return 0;
    }
}

} // namespace gamemodes

