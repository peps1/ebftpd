#ifndef __CFG_SETTING_HPP
#define __CFG_SETTING_HPP

#include <string>
#include <vector>
#include <memory>
#include <boost/regex_fwd.hpp>
#include <sys/types.h>
#include "acl/acl.hpp"
#include "acl/passwdstrength.hpp"
#include "acl/ipstrength.hpp"
#include "main.hpp"

namespace boost { namespace posix_time
{
class seconds;
}
}

namespace cfg
{

class Database
{
  std::string name;
  std::string address;
  std::string host;
  int port;
  std::string login;
  std::string password;
  
public:
  Database();
  Database(const std::vector<std::string>& toks);
  
  const std::string& Name() const { return name; }
  const std::string& Address() const { return address; }
  int Port() const { return port; }
  const std::string& Host() const { return host; }
  const std::string& Login() const { return login; }
  const std::string& Password() const { return password; }
  bool NeedAuth() const;
};

class Right
{
  std::string path;
  // includes wildcards and possibley regex so can't be std::string path;
  acl::ACL acl;
  bool specialVar;
  
public:
  Right(std::vector<std::string> toks);
  const acl::ACL& ACL() const { return acl; }
  const std::string& Path() const { return path; }
  bool SpecialVar() const { return specialVar; }
};

class ACLInt
{
  int arg;
  acl::ACL acl;
  
public:
  ACLInt(std::vector<std::string> toks);
  const acl::ACL& ACL() const { return acl; }
  int Arg() const { return arg; }
};

class AsciiDownloads
{
  long long kBytes;
  std::vector<std::string> masks;
  
public:
  AsciiDownloads() : kBytes(-1) { }
  AsciiDownloads(const std::vector<std::string>& toks);
  bool Allowed(off_t size, const std::string& path) const;
};

class AsciiUploads
{
  std::vector<std::string> masks;
  
public:
  AsciiUploads() = default;
  AsciiUploads(const std::vector<std::string>& toks);
  bool Allowed(const std::string& path) const;
};

class SecureIp
{
  acl::IPStrength strength;
  acl::ACL acl;

public:
  SecureIp(std::vector<std::string> toks);
  const acl::IPStrength& Strength() const { return strength; }
  const acl::ACL& ACL() const { return acl; }
};

class SecurePass
{
  acl::PasswdStrength strength;
  acl::ACL acl;
  
public:
  SecurePass(std::vector<std::string> toks);
  const acl::ACL& ACL() const { return acl; }
  const acl::PasswdStrength& Strength() const { return strength; }
};

class BouncerIp
{
  std::vector<std::string> addrs;
  
public:
  BouncerIp(const std::vector<std::string>& toks);
  const std::vector<std::string>& Addrs() const { return addrs; }
};

class SpeedLimit
{
  std::string path;
  long long downloads;
  long long uploads;
  acl::ACL acl;
  
public:
  SpeedLimit(std::vector<std::string> toks);
  const std::string& Path() const { return path; }
  long long Uploads() const { return downloads; }
  long long Downloads() const { return uploads; }
  const acl::ACL& ACL() const { return acl; }
};

class SimXfers
{
  int maxDownloads;
  int maxUploads;
  
public:
  SimXfers() : maxDownloads(-1), maxUploads(-1) { }
  SimXfers(std::vector<std::string> toks);
  int MaxDownloads() const { return maxDownloads; }
  int MaxUploads() const { return maxUploads; }
};

class PasvAddr
{
  std::string addr;
  
public:
  PasvAddr(const std::vector<std::string>& toks);
  const std::string& Addr() const { return addr; }
};

class PortRange
{
  int from;
  int to;

public:
  PortRange(int from, int to) : from(from), to(to) { }
  int From() const { return from; }
  int To() const { return to; }
};
 
class Ports
{
  std::vector<PortRange> ranges;
  
public:
  Ports() = default;
  Ports(const std::vector<std::string>& toks);
  const std::vector<PortRange>& Ranges() const { return ranges; }
};

class AllowFxp
{
  bool downloads;
  bool uploads;
  bool logging;
  acl::ACL acl;
  
public:
  AllowFxp() :
    downloads(true), uploads(true), 
    logging(false), acl("*")
  { }
  
  AllowFxp(std::vector<std::string> toks);
  bool Downloads() const { return downloads; }
  bool Uploads() const { return uploads; }
  bool Logging() const { return logging; }
  const acl::ACL& ACL() const { return acl; }  
};

class Alias
{
  std::string name;
  std::string path;
  
public:
  Alias(const std::vector<std::string>& toks);
  const std::string& Name() const { return name; }
  const std::string& Path() const { return path; }
};

class PathFilter
{
  std::unique_ptr<boost::regex> regex;
  acl::ACL acl;
  
public:
  PathFilter& operator=(const PathFilter& rhs);
  PathFilter& operator=(PathFilter&& rhs);
  PathFilter(const PathFilter& other);
  PathFilter(PathFilter&& other);
  ~PathFilter();
  
  PathFilter();
  PathFilter(std::vector<std::string> toks);
  const boost::regex& Regex() const;
  const acl::ACL& ACL() const { return acl; }
  
};

class MaxUsers
{
  int users;
  int exemptUsers;
  
public:
  MaxUsers() : users(50), exemptUsers(5) { }
  MaxUsers(const std::vector<std::string>& toks);
  int Users() const { return users; }
  int ExemptUsers() const { return exemptUsers; }
  int Total() const { return users + exemptUsers; }
};

class Lslong
{
  std::string options;
  int maxRecursion;
  
public:
  Lslong() : options("l"), maxRecursion(2) { }
  Lslong(std::vector<std::string> toks);
  const std::string& Options() const { return options; }
  int MaxRecursion() const { return maxRecursion; }
};

class HiddenFiles
{
  std::string path;
  std::vector<std::string> masks;
  
public:
  HiddenFiles(std::vector<std::string> toks);
  const std::string& Path() const { return path; }
  const std::vector<std::string>& Masks() const { return masks; }
};

class Requests
{
  std::string path;
  int max;
  
public:
  Requests() : max(10) { }
  Requests(const std::vector<std::string>& toks);
  const std::string& Path() const { return path; }                              
  int Max() const { return max; }
};

class Creditcheck
{
  std::string path;
  int ratio;
  acl::ACL acl;
  
public:
  Creditcheck(std::vector<std::string> toks);
  const std::string& Path() const { return path; }
  int Ratio() const { return ratio; }
  const acl::ACL& ACL() const { return acl; }
};

class Creditloss
{
  std::string path;
  int ratio;
  acl::ACL acl;
  
public:
  Creditloss(std::vector<std::string> toks);
  const acl::ACL& ACL() const { return acl; }
  int Ratio() const { return ratio; }
  const std::string& Path() const { return path; }
};

class NukedirStyle
{
public:
  enum Action { DeleteAll, DeleteFiles, Keep };

private:
  std::string format;
  Action action;
  long long emptyKBytes;

public:
  NukedirStyle();
  NukedirStyle(const std::vector<std::string>& toks);
  const std::string& Format() const { return format; }
  long long EmptyKBytes() const { return emptyKBytes; }
  Action GetAction() const { return action; }
};

class Msgpath
{
  std::string path;
  std::string filepath;
  acl::ACL acl;
  
public:
  Msgpath(const std::vector<std::string>& toks);
  const std::string& Path() const { return path; }
  const acl::ACL& ACL() const { return acl; }
  const std::string& Filepath() const { return filepath; }
};

class Privpath
{
  std::string path; // no wildcards to avoid slowing down listing
  acl::ACL acl;

public:
  Privpath(std::vector<std::string> toks);
  const std::string& Path() const { return path; }
  const acl::ACL& ACL() const { return acl; }
};

class SiteCmd
{
public:
  enum class Type { Exec, Text, Alias };
  
private:
  std::string command;
  Type type;
  std::string description;
  std::string target;
  std::string arguments;

public:
  SiteCmd(const std::vector<std::string>& toks);
  const std::string& Command() const { return command; }
  Type GetType() const { return type; }
  const std::string& Description() const { return description; }
  const std::string& Arguments() const { return arguments; }
  const std::string& Target() const { return target; }
};

class Cscript
{
public:
  enum class Type { Pre, Post };

private:
  std::string command;
  Type type;
  std::string path;
  
public:
  Cscript(const std::vector<std::string>& toks);
  const std::string& Command() const { return command; }                       
  const std::string& Path() const { return path; }
  Type GetType() const { return type; }
};

struct IdleTimeoutImpl;

class IdleTimeout
{
  std::unique_ptr<IdleTimeoutImpl> pimpl;
  
public:
  IdleTimeout& operator=(const IdleTimeout& rhs);
  IdleTimeout& operator=(IdleTimeout&& rhs);
  IdleTimeout(const IdleTimeout& other);
  IdleTimeout(IdleTimeout&& other);

  IdleTimeout();
  IdleTimeout(const std::vector<std::string>& toks);
  ~IdleTimeout();
    
  boost::posix_time::seconds Maximum() const;
  boost::posix_time::seconds Minimum() const;
  boost::posix_time::seconds Timeout() const;
};

class CheckScript
{
  std::string path;
  std::string mask;
  bool disabled;

public:
  CheckScript(const std::vector<std::string>& toks);

  const std::string Path() const { return path; }
  const std::string Mask() const { return mask; }
  bool Disabled() const { return disabled; }
};

class Log
{
  std::string name;
  bool console;
  bool file;
  long database;
  
public:
  Log(const std::string& name, bool console, bool file, long database) : 
    name(name), console(console), file(file), database(database)
  { }
  
  Log(const std::string& name, const std::vector<std::string>& toks);
  
  const std::string& Name() const { return name; }
  bool Console() const { return console; }
  bool File() const { return file; }
  bool Database() const { return database > 0; };
  long CollectionSize() const { return database; }
};

class TransferLog : public Log
{
  bool uploads;
  bool downloads;
  
public:
  TransferLog(const std::string& name, bool console, bool file, 
              long database, bool uploads, bool downloads) : 
    Log(name, console, file, database),
    uploads(uploads), downloads(downloads)
  { }
  
  TransferLog(const std::string& name, const std::vector<std::string>& toks);
  
  bool Uploads() const { return uploads; }
  bool Downloads() const { return downloads; }
};

}

#endif
