
#include <map>
#include <vector>

#include "faction.h"
#include "logging.h"
#include "stringtools.h"

namespace factions
{

std::vector<std::string> __names = { "Soviet", "Allied", "Yuri", "All/Sov" };
std::vector<std::string> __shortNames = { "sov", "all", "yur", "mix" };
std::vector<std::string> __letters = { "s", "a", "y", "m" };

/*!
 */
std::string name(Faction faction)
{
    return __names[faction % count()];
}

/*!
 */
std::string shortName(Faction faction)
{
    return __shortNames[faction % count()];
}

/*!
 */
std::string letter(Faction faction)
{
    return __letters[faction % count()];
}

/*!
 */
Faction fromName(const std::string &name)
{
    std::string lowered = stringtools::toLower(name);

    if (lowered.contains("greece") || lowered.contains("turkey") || lowered.contains("england") || lowered.contains("spain") || lowered.contains("france") || lowered.contains("germany") || lowered.contains("america") || lowered.contains("korea") || lowered.contains("britain"))
    {
        return Allied;
    }
    else if (lowered.contains("ukraine") || lowered.contains("iraq") || lowered.contains("russia") || lowered.contains("cuba") || lowered.contains("libya"))
    {
        return Soviet;
    }
    else if (lowered.contains("yuri"))
    {
        return Yuri;
    }
    else
    {
        return UnknownFaction;
    }
}

/*!
 */
SpecialFaction specialFactionFromName(const std::string &name)
{
    static std::map<std::string, SpecialFaction> factions = {
        { "iraq", SpecialFaction::Iraq },
        { "britain", SpecialFaction::Britain },
        { "france", SpecialFaction::France },
        { "america", SpecialFaction::America },
        { "germany", SpecialFaction::Germany },
        { "korea", SpecialFaction::Korea },
        { "russia", SpecialFaction::Russia },
        { "cuba", SpecialFaction::Cuba },
        { "russia", SpecialFaction::Russia },
        { "libya", SpecialFaction::Libya }
    };

    std::string lowered = stringtools::toLower(name);

    if (!factions.contains(lowered))
    {
        return SpecialFaction::UnknownSpecialFaction;
    }
    else
    {
        return factions[lowered];
    }
}

/*!
 */
Faction fromShortName(const std::string &shortName)
{
    auto it = std::find(__shortNames.begin(), __shortNames.end(), shortName);

    if (it != __shortNames.end())
    {
        int index = static_cast<int>(std::distance(__shortNames.begin(), it));
        return toFaction(index);
    }

    return UnknownFaction;
}


Setup fromFactions(Faction faction1, Faction faction2)
{
    if (faction1 == Soviet)
    {
        switch (faction2)
        {
        case Allied:
            return SvA;
        case Soviet:
            return SvS;
        case Yuri:
            return SvY;
        default:
            Log::fatal() << "Unknown faction setup.";
            return UnknownSetup;
        }
    }
    else if (faction1 == Allied)
    {
        switch (faction2)
        {
        case Allied:
            return factions::AvA;
        case Soviet:
            return AvS;
        case Yuri:
            return AvY;
        default:
            Log::fatal() << "Unknown faction setup.";
            return UnknownSetup;
        }
    }
    else if (faction1 == factions::Yuri)
    {
        switch (faction2)
        {
        case Allied:
            return YvA;
        case factions::Soviet:
            return YvS;
        case factions::Yuri:
            return YvY;
        default:
            Log::fatal() << "Unknown faction setup.";
            return UnknownSetup;
        }
    }

    Log::fatal() << "Unknown faction setup.";
    return UnknownSetup;
}

std::string toString(Setup setup)
{
    switch(setup)
    {
    case AvA:
        return "AvA";
    case AvS:
        return "AvS";
    case SvA:
        return "SvA";
    case SvS:
        return "SvS";
    case YvA:
        return "YvA";
    case YvS:
        return "YvS";
    case AvY:
        return "AvY";
    case SvY:
        return "SvY";
    case YvY:
        return "YvY";
    default:
        return "???";
    }
}

Faction firstFaction(Setup setup)
{
    switch(setup)
    {
    case AvA:
    case AvS:
    case AvY:
        return Allied;
    case SvA:
    case SvY:
    case SvS:
        return Soviet;
    case YvA:
    case YvS:
    case YvY:
        return Yuri;
    default:
        return UnknownFaction;
    }
}

Faction secondFaction(Setup setup)
{
    switch(setup)
    {
    case AvA:
    case SvA:
    case YvA:
        return Allied;
    case AvS:
    case SvS:
    case YvS:
        return Soviet;
    case AvY:
    case SvY:
    case YvY:
        return Yuri;
    default:
        return UnknownFaction;
    }
}


} // namespace factions
