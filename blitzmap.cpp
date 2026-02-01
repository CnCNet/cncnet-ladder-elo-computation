
#include <algorithm>
#include <string>
#include <vector>
#include <cctype>
#include <iostream>
#include <sstream>
#include <map>

#include "blitzmap.h"
#include "logging.h"
#include "stringtools.h"

std::array<std::string, blitzmap::MapCount> blitzmap::__names = {
    "Alamo", "Kong", "Big Little Lake", "Castle", "Oasis", "Doom", "Yin Yang", "Brute", "Mummy", "Surge",
    "Prime", "Demo", "Spark", "Carnival", "Bongo", "Boom", "Texas", "Volley", "River Riot", "Toothpick",
    "Tundra", "King's Hill", "Dry Heat", "Pirate Bay", "Breaking Bad", "Skyrim Shot", "Quick Sand", "Paika BLITZ",
    "Doofus Omnibus", "The Burg", "Downhill Rush", "Cloud Nine", "LgndFan", "Dune II", "Momento",
    "Revenant", "Chimp Frenzy", "Equinox", "RIP Jaws", "Path More Traveled", "Jeen Strike", "Thunder Dome",
    "Caladan", "Campgrounds", "Night Shade", "Paradropia"
};

std::array<std::string, blitzmap::MapCount> blitzmap::__shortNames = {
    "alamo", "kong", "lake", "castle", "oasis", "doom", "yinyang", "brute", "mummy", "surge",
    "prime", "demo", "spark", "carnival", "bongo", "boom", "texas", "volley", "river riot", "toothpick",
    "tundra", "kingshill", "dryheat", "piratebay", "breaking bad", "skyrim shot", "quick sand", "paika",
    "omnibus", "theburg", "downhillrush", "cloudnine", "lgndfan", "dune2", "momento", "revenant",
    "chimpfrenzy", "equinox", "ripjaws", "pathtraveled", "jeenstrike", "thunderdome", "caladan",
    "campgrounds", "night shade", "paradropia"
};

std::array<blitzmap::MapSize, blitzmap::MapCount> blitzmap::__mapSizes = {
    Small, Small, Small, Small, Small, Small, Small, Small, Small, Small,
    Small, Small, Small, Large, Medium, Medium, Medium, Large, Large, Medium,
    Large, Medium, Medium, Medium, Medium, Medium, Small, Small,
    Medium, Medium, Large, Small, Small, Medium, Medium, Large,
    Small, Large, Large, Large, Small, Medium, Medium,
    Small, Small, Medium, Small
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

    std::map<std::string, std::string> hashes = {
        { "53be9c689e6b5a25642d1adb69c56aceec50dd47", "Night Shade" },
        { "e4a94cbb2a5e85f83c982fa06d18855d86bc8894", "Caladan" },
        { "be6f8f4d69e941fa3b76174594156b6bb463f6a6", "Doom" },
        { "bd7cb89cd4b486f203d5e506d6c33cf1b81a592c", "Campground" },
        { "b6db3d761db8d9818974e8b17a2635f534518680", "Castle" },
        { "f9ae50a84d33f7d9ef302d9f2ef806576456c185", "Jeen Strike" },
        { "e8b15d005f1e7a6712d5320498fa35724dde422e", "King's Hill" },
        { "42645aaec42594c7964792be0b5ecb0046a5a7c1", "Skyrim Shot" },
        { "aa585274e3d2eeeb014d3027d987f91e236b8a8b", "The Path More Traveled" },
        { "8a18203451c54e0bdccc0569ac9f8588bb932576", "Cloud Nine" },
        { "4e2b056bf4961ffbdfbd53fc1b4a36a3311e631c", "Spark" },
        { "b08e876c71dc52b1d6fe93032a60c4ab69213226", "Breaking Bad" },
        { "b26fb3842ba5cdc421120e9af722cbf91cb53499", "Brute" },
        { "7be2fa19636e31fbe65dbddb6ccbad8eeca47e45", "Prime" },
        { "3ed2a5bf3203b16ebf8871d4c46d29740466ab01", "?????" },
        { "35955d74bb1df2c282bf66ceadaf8a69e903f617", "?????" },
        { "2dffafc965453e412a0b1431f3abde6650e8c299", "?????" },
        { "c17ca4fe03964e302d1cf30f0909740c9a5f14b6", "?????" },
        { "3f989ec10e3ff5ab4e447831cc4d8086540be8c7", "?????" },
        { "30ec7643226caa269d3d7c3a6e4abe9d8b548d5c", "?????" },
        { "5cf997e6f0d1567aa3d188ef0aee1d055f7de652", "?????" },
        { "0e20ec98ebc8a2fcaf65e94355cc3435a22b625b", "?????" },
        { "2d322737d77030629077d62d4b4caee7921bbead", "?????" },
        { "2b8ea601a69da2c9eb52a1a007b8b6cb4fb641c2", "?????" },
        { "f7cb6c8146e88d41f3d05e0b189a65dfac799bc6", "?????" },
        { "44e76c5507fd53199d0c8ffa7f246c92e0b117ec", "?????" },
        { "308592a1f20251165a22c6162d433876fdefffcb", "?????" },
        { "3c18e314c5407a07f251a5dd349cdeef5e72bd91", "?????" },
        { "92f86a63cf441282e6558f005da4881385a6e350", "?????" },
        { "efe62be9f0862a1998f591a01aff1d65771ea463", "?????" },
        { "7cd3ce633489f876ef1b63503b7974c7ece3228b", "?????" },
        { "11fdb0e5e1b11288b4a7226ea2c0d14c15a1b813", "?????" },
        { "2a0adb389319af86642b2758a798df40556cc609", "?????" },
        { "14914cccaac6184e55b3e789a0efbb391d96541c", "?????" }
    };

    if (hashes.find(trimmedName) != hashes.end())
    {
        trimmedName = hashes[trimmedName];
    }

    std::string sub = " Dominator";

    auto pos = trimmedName.find(sub);
    if (pos != std::string::npos) {
        trimmedName.erase(pos, sub.size());
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
                standardCase = trimmedName.substr(spacePosition + 1, secondSpace);
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
