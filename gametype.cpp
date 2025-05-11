 

#include "gametype.h"

namespace gametypes
{

std::vector<std::string> __names = { "Unknown", "Quick Match", "Show Match", "World Series" };
std::vector<std::string> __shortNames = { "?", "qm", "show", "ws" };

/*!
 */
std::string name(GameType gameType)
{
    return __names[std::min<uint32_t>(gameType, count())];
}

/*!
 */
std::string shortName(GameType gameType)
{
    return __shortNames[std::min<uint32_t>(gameType, count())];
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
GameType toGameType(const std::string &name)
{
    int index = toIndex(name);

    if (index < 0 || index > count())
    {
        index = 0;
    }

    return static_cast<GameType>(index);
}

} // namespace gametypes

