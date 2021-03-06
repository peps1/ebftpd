#ifndef __STATS_UTIL_HPP
#define __STATS_UTIL_HPP

#include <boost/optional/optional_fwd.hpp>
#include <cmath>

namespace boost { namespace posix_time
{
class ptime;
class time_duration;
}
}

namespace cfg
{
class  Section;
}

namespace ftp
{
class Client;
}

namespace fs
{
class VirtualPath;
}

namespace cfg
{
class Section;
}

namespace stats
{

double CalculateSpeed(long long bytes, const boost::posix_time::time_duration& duration);
double CalculateSpeed(long long bytes, const boost::posix_time::ptime& start, 
        const boost::posix_time::ptime& end);

boost::posix_time::time_duration SpeedLimitSleep(
      const boost::posix_time::time_duration& xfertime, 
      long long bytes, long long limitBytes);
        
std::string AutoUnitSpeedString(double speed);
std::string AutoUnitString(double kBytes);
std::string HighResSecondsString(const boost::posix_time::time_duration& duration);
std::string HighResSecondsString(const boost::posix_time::ptime& start, 
        const boost::posix_time::ptime& end);
int UploadRatio(const ftp::Client& client, const fs::VirtualPath& path, 
    const boost::optional<const cfg::Section&>& section);
int DownloadRatio(const ftp::Client& client, const fs::VirtualPath& path, 
    const boost::optional<const cfg::Section&>& section);
    
} /* stats namespace */

#endif
