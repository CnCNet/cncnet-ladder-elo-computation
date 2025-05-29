 
#include <iomanip>

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

std::string stringtools::fromDateTime(const std::chrono::system_clock::time_point &tp)
{
    using namespace std::chrono;

    // Datum extrahieren
    sys_days date = floor<days>(tp);
    year_month_day ymd{date};

    // Uhrzeit extrahieren (Zeit seit Mitternacht)
    hh_mm_ss hms{tp - date};

    // Formatieren
    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(4) << int(ymd.year()) << '-'
       << std::setw(2) << unsigned(ymd.month()) << '-'
       << std::setw(2) << unsigned(ymd.day()) << '@'
       << std::setw(2) << hms.hours().count() << ':'
       << std::setw(2) << hms.minutes().count() << '.'
       << std::setw(2) << hms.seconds().count();

    return ss.str();
}
