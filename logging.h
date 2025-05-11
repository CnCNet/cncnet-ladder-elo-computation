 

#pragma once

#include <iostream>
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
        Count = 7

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

    static Log verbose()
    {
        return Log(Verbose);
    }

    static Log info()
    {
        return Log(Info);
    }

    static Log warning()
    {
        return Log(Warning);
    }

    static Log error()
    {
        return Log(Error);
    }

    static Log critical()
    {
        return Log(Critical);
    }

    static Log fatal()
    {
        return Log(Fatal);
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
            Log::error() << "Unknown log level '" << level << "'. Returning 'warning'.";
            return Log::Warning;
        }
    }

private:
    //! The log level.
    Level _level;

    //! The output string stream.
    std::ostringstream _os;

}; // class Log
