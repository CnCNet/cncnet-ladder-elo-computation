
#pragma once

#include <algorithm>
#include <string>

namespace factions
{

enum Faction
{
    Soviet = 0,
    Allied = 1,
    Yuri = 2,
    Combined = 3,
    UnknownFaction = 4
};

enum SpecialFaction
{
    UnknownSpecialFaction = 0,
    Libya = 1,
    Cuba = 2,
    Russia = 3,
    Iraq = 4,
    Germany = 5,
    America = 6,
    Britain = 7,
    Korea = 8,
    France = 9
};

enum Setup
{
    SvS = 0,
    AvS = 1,
    SvA = 2,
    AvA = 3,
    SvY = 4,
    YvS = 5,
    AvY = 6,
    YvA = 7,
    YvY = 8,
    UnknownSetup = 9
};

// Constant expressions.
inline constexpr int count()
{
    return 4;
}

std::string name(Faction faction);
std::string shortName(Faction faction);
std::string letter(Faction faction);

inline constexpr Faction toFaction(int i)
{
    return static_cast<Faction>(i);
}

Faction fromName(const std::string &name);
SpecialFaction specialFactionFromName(const std::string &name);

//! Get the faction from short names (used for json).
Faction fromShortName(const std::string &shortName);

extern Setup fromFactions(Faction faction1, Faction faction2);
extern std::string toString(Setup setup);
extern Faction firstFaction(Setup setup);
extern Faction secondFaction(Setup setup);


} // namespace factions

