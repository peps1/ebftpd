#include <vector>
#include "fs/path.hpp"
#include "cfg/get.hpp"
#include "util/string.hpp"
#include "ftp/client.hpp"
#include "ftp/xdupe.hpp"
#include "logs/logs.hpp"
#include "fs/dircontainer.hpp"

namespace ftp { namespace xdupe
{

std::string MessageOne(const std::vector<std::string>& dupes)
{
  static const unsigned maxLength = 67;

  std::ostringstream reply;
  std::ostringstream line;
  std::string::size_type len = 0;
  
  line << "X-DUPE: ";
  for (const std::string& dupe : dupes)
  {
    if (dupe.length() > maxLength)
    {
      if (len > 0) reply << line.str() << '\n';
      reply << "X-DUPE: " << dupe.substr(0, maxLength) << '\n';
      line.str("");
      line << "X-DUPE: ";
      len = 0;
      continue;
    }

    if (len + dupe.length() + (len > 0 ? 1 : 0) > maxLength)
    {
      reply << line.str() << '\n';
      line.str("");
      line << "X-DUPE: ";
      len = 0;
    }
    
    if (len > 0)
    {
      line << ' ';
      ++len;
    }
    
    line << dupe;
    len += dupe.length();
  }
  
  if (len > 0) reply << line.str() << '\n';
  return reply.str();
}

std::string MessageTwo(const std::vector<std::string>& dupes)
{
  static const unsigned maxLength = 75;
  
  std::ostringstream reply;
  for (const std::string& dupe : dupes)
  {
    reply << "X-DUPE: " << dupe.substr(0, maxLength) << '\n';
  }
  
  return reply.str();
}

std::string MessageThree(const std::vector<std::string>& dupes)
{
  std::ostringstream reply;
  for (const std::string& dupe : dupes)
  {
    reply << "X-DUPE: " << dupe << '\n';
  }
  
  return reply.str();
}

std::string MessageFour(const std::vector<std::string>& dupes)
{
  static const unsigned maxLength = 1016;
  
  std::ostringstream reply;
  std::string::size_type len = 8;
  
  reply << "X-DUPE: ";
  for (const std::string& dupe : dupes)
  {
    if (len + dupe.length() + (len == 8 ? 0 : 1) > maxLength) break;
    if (len != 8)
    {
      reply << ' ';
      ++len;
    }
    reply << dupe;
    len += dupe.length();
  }
  
  const std::string& s = reply.str();
  if (s.length() == 8) return std::string("");
  return s + '\n';
}

bool IsXdupe(const fs::Path& path)
{
  const std::string& pathStr = path.ToString();
  for (const std::string& mask : cfg::Get().Xdupe())
  {
    if (util::string::WildcardMatch(mask, pathStr)) return true;
  }
  return false;
}

std::vector<std::string> BuildDupeList(ftp::Client& client, const fs::VirtualPath& path)
{
  std::vector<std::string> dupes;
  try
  {
    for (const fs::Path& dupe : fs::DirContainer(client, path.Dirname()))
    {
      if (IsXdupe(dupe)) dupes.push_back(dupe.ToString());
    }
  }
  catch (const util::SystemError& e)
  {
    logs::error << "Error while building xdupe list: " << e.Message() << logs::endl;
  }
  return dupes;
}

std::vector<std::function<std::string(const 
      std::vector<std::string>& dupes)>> modeFunctions =
{
  nullptr,
  &MessageOne,
  &MessageTwo,
  &MessageThree,
  &MessageFour
};

std::string Message(ftp::Client& client, const fs::VirtualPath& path)
{
  const auto& function = modeFunctions[static_cast<unsigned>(client.XDupeMode())];
  if (!function) return "";
  
  return function(BuildDupeList(client, path));
}

} /* xdupe namespace */
} /* ftp namespace */
