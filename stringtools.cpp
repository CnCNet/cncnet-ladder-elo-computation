 
#include "stringtools.h"


std::string stringtools::toLower(const std::string &str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

std::string stringtools::toUpper(const std::string &str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return result;
}

std::string stringtools::trimmed(const std::string &str)
{
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");

    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

std::string stringtools::fromDate(const std::chrono::year_month_day &date)
{
    std::stringstream ss;

    ss << int(date.year()) << '-';

    if (static_cast<unsigned>(date.month()) < 10)
        ss << "0";

    ss << static_cast<unsigned>(date.month()) << "-";

    if (static_cast<unsigned>(date.day()) < 10)
        ss << "0";

    ss << static_cast<unsigned>(date.day());

    return ss.str();
}
