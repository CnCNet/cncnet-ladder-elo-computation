 
#pragma once

#include <cctype>
#include <chrono>
#include <sstream>
#include <string>

namespace stringtools
{

extern std::string toLower(const std::string &str);
extern std::string toUpper(const std::string &str);
extern std::string trimmed(const std::string &str);
extern std::string fromDate(const std::chrono::year_month_day &date);
extern std::string fromDateTime(const std::chrono::system_clock::time_point &tp);

template <typename Container>
std::string join(const Container& container, const std::string &delimiter = ", ")
{
    std::ostringstream oss;
    auto it = std::begin(container);
    if (it != std::end(container))
    {
        oss << *it;
        ++it;
    }
    while (it != std::end(container))
    {
        oss << delimiter << *it;
        ++it;
    }
    return oss.str();
}

} // namespace stringtools
