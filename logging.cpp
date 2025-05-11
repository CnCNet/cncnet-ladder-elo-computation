
#include <iostream>

#include "logging.h"


Log::Level Log::_globalLogLevel = Log::Level::Info;
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
    if (_globalLogLevel <= this->_level && Log::_enabled)
    {
        std::cout << _os.str() << std::endl;
    }
}
