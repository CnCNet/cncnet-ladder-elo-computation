
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>

#include "logging.h"


Log::Level Log::_globalLogLevel = Log::Level::Info;
bool Log::_showTimestampAndLogLevel = false;
bool Log::_enabled = true;

/*!
 */
Log::Log(Level level) :
    _level(level)
{
}

/*!
 */
Log::~Log()
{
    static std::map<Level, std::string> map{
        { Debug, "DEBUG"},
        { Verbose, "VERBOSE" },
        { Info, "INFO" },
        { Warning, "WARNING" },
        { Error, "ERROR" },
        { Critical, "CRITICAL" },
        { Fatal, "FATAL" }
    };

    if (this->_level != Log::NoLog && _globalLogLevel <= this->_level && Log::_enabled)
    {
        if (this->_showTimestampAndLogLevel)
        {
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            std::tm* tm_utc = std::gmtime(&now_time_t);

            std::cout << std::put_time(tm_utc, "%Y-%m-%d@%H.%M.%S")
                      << '.' << std::setfill('0') << std::setw(3) << now_ms.count()
                      << " [" << map[this->_level] << "] ";
        }

        std::cout << _os.str() << std::endl;
    }
}
