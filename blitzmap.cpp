
#include <algorithm>
#include <string>
#include <vector>
#include <cctype>
#include <iostream>
#include <sstream>

#include "blitzmap.h"
#include "logging.h"
#include "stringtools.h"

std::array<std::string, blitzmap::MapCount> blitzmap::__names = {
    "Alamo", "Kong", "Big Little Lake", "Castle", "Oasis", "Doom", "Yin Yang", "Brute", "Mummy", "Surge",
    "Prime", "Demo", "Spark", "Carnival", "Bongo", "Boom", "Texas", "Volley", "River Riot", "Toothpick",
    "Tundra", "King's Hill", "Dry Heat", "Pirate Bay", "Breaking Bad", "Skyrim Shot", "Quick Sand", "Paika BLITZ",
    "The Doofus Omnibus", "The Burg", "Downhill Rush", "Cloud Nine", "LgndFan", "Dune II", "Momento",
    "Revenant", "Chimp Frenzy", "Equinox", "RIP Jaws", "The Path More Traveled", "Jeen Strike", "Thunder Dome",
    "Caladan", "Campgrounds", "Night Shade"
};

std::array<std::string, blitzmap::MapCount> blitzmap::__shortNames = {
    "alamo", "kong", "lake", "castle", "oasis", "doom", "yinyang", "brute", "mummy", "surge",
    "prime", "demo", "spark", "carnival", "bongo", "boom", "texas", "volley", "river riot", "toothpick",
    "tundra", "kingshill", "dryheat", "piratebay", "breaking bad", "skyrim shot", "quick sand", "paika",
    "omnibus", "theburg", "downhillrush", "cloudnine", "lgndfan", "dune2", "momento", "revenant",
    "chimpfrenzy", "equinox", "ripjaws", "pathtraveled", "jeenstrike", "thunderdome", "caladan",
    "campgrounds", "night shade"
};

std::array<blitzmap::MapSize, blitzmap::MapCount> blitzmap::__mapSizes = {
    Small, Small, Small, Small, Small, Small, Small, Small,
    Small, Small, Small, Small, Small, Large, Medium, Medium,
    Medium, Large, Large, Medium, Large, Medium, Medium, Medium,
    Medium, Medium, Small, Small, Medium, Medium, Large, Small,
    Small, Medium, Medium, Large, Small, Large, Large, Large, Small,
    Medium, Medium, Small, Small
};

/*!
 */
int blitzmap::toIndex(const std::string &name)
{
    std::string trimmedName = stringtools::trimmed(name);

    if (trimmedName.empty())
    {
        Log::error() << "Passed empty map name.";
        return -1;
    }

    std::string standardCase = trimmedName;
    size_t spacePosition = trimmedName.find(' ');

    if (spacePosition != std::string::npos)
    {
        standardCase = trimmedName.substr(0, spacePosition);
        if (standardCase == "The")
        {
            size_t secondSpace = trimmedName.find(' ', spacePosition + 1);
            if (secondSpace != std::string::npos)
            {
                standardCase = trimmedName.substr(0, secondSpace);
            }
            else
            {
                // Use full string if there is no second word.
                standardCase = trimmedName;
            }
        }
    }

    std::string lowerCase = stringtools::toLower(standardCase);

    for (size_t i = 0; i < blitzmap::__names.size(); ++i)
    {
        if (blitzmap::__names[i].rfind(standardCase, 0) == 0 || blitzmap::__names[i].rfind(lowerCase, 0) == 0)
        {
            return static_cast<int>(i);
        }

        if (blitzmap::__shortNames[i].rfind(standardCase, 0) == 0 || blitzmap::__shortNames[i].rfind(lowerCase, 0) == 0)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}
