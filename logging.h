
#pragma once

#include <chrono>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

class Log
{
public:
    //! Log level enum.
    enum Level
    {
        Debug = 0,
        Verbose = 1,
        Info = 2,
        Warning = 3,
        Error = 4,
        Critical = 5,
        Fatal = 6,
        Count = 7,
        NoLog = 8

    }; // enum Level

public:
    //! Move constructor.
    inline Log(Log&& other) :
        _level(other._level)
    {
        if (Log::_enabled)
        {
            _os << other._os.str();
        }
    }

    //! Destructor. Sends the log message to the global logging.
    ~Log();

private:
    //! Use private constructor for singleton class.
    Log(Level level);

public:
    static Log debug()
    {
        return Log(Debug);
    }

    static Log debug(bool condition)
    {
        return condition ? Log(Debug) : Log(NoLog);
    }

    static Log verbose()
    {
        return Log(Verbose);
    }

    static Log verbose(bool condition)
    {
        return condition ? Log(Verbose) : Log(NoLog);
    }

    static Log info()
    {
        return Log(Info);
    }

    static Log info(bool condition)
    {
        return condition ? Log(Info) : Log(NoLog);
    }

    static Log warning()
    {
        return Log(Warning);
    }

    static Log warning(bool condition)
    {
        return condition ? Log(Warning) : Log(NoLog);
    }

    static Log error()
    {
        return Log(Error);
    }

    static Log error(bool condition)
    {
        return condition ? Log(Error) : Log(NoLog);
    }

    static Log critical()
    {
        return Log(Critical);
    }

    static Log critical(bool condition)
    {
        return condition ? Log(Critical) : Log(NoLog);
    }

    static Log fatal()
    {
        return Log(Fatal);
    }

    static Log fatal(bool condition)
    {
        return condition ? Log(Fatal) : Log(NoLog);
    }

    static Log noLog()
    {
        return Log(NoLog);
    }

    //! Stream operator.
    template <typename T>
    Log& operator<<(const T& t)
    {
        if (Log::_enabled)
        {
            _os << t;
        }
        return *this;
    }

    template <typename T>
    Log& operator<<(const std::set<T> &values)
    {
        bool isFirst = true;
        _os << "[";

        for (T value : values)
        {
            if (isFirst)
            {
                _os << value;
                isFirst = false;
            }
            else
            {
                _os << ", " << value;
            }
        }

        _os << "]";

        return *this;
    }

    Log& operator<<(const std::chrono::year_month_day &date)
    {
        if (Log::_enabled)
        {
            _os << int(date.year()) << '-';

            if (static_cast<unsigned>(date.month()) < 10)
                _os << "0";

            _os << static_cast<unsigned>(date.month()) << "-";

            if (static_cast<unsigned>(date.day()) < 10)
                _os << "0";

            _os << static_cast<unsigned>(date.day());
        }
        return *this;
    }

    //! The global log level.
    static Level _globalLogLevel;

    //! Log enabled or disabled?
    static bool _enabled;

    //! Set the global log level.
    static void setGlobalLogLevel(Level level)
    {
        Log::_globalLogLevel = level;
    }

    //! Enable or disable the log.
    static void setEnabled(bool enabled)
    {
        Log::_enabled = enabled;
    }

    //! Convert a string to a log level.
    static Log::Level fromString(const std::string &level)
    {
        static std::vector<std::string> loglevels{ "debug", "verbose", "info", "warning", "error", "critical", "fatal" };
        auto it = std::find(loglevels.begin(), loglevels.end(), level);

        if (it != loglevels.end())
        {
            return static_cast<Log::Level>(static_cast<int>(std::distance(loglevels.begin(), it)));
        }
        else
        {
            return Log::NoLog;
        }
    }

    //! Set flag for details information about every log line.
    static void addTimestampAndLogLevel(bool enabled)
    {
        Log::_showTimestampAndLogLevel = enabled;
    }

private:
    //! The log level.
    Level _level;

    //! Do we want timestamps and log levels?
    static bool _showTimestampAndLogLevel;

    //! The output string stream.
    std::ostringstream _os;

}; // class Log
