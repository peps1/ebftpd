//    Copyright (C) 2012, 2013 ebftpd team
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <sstream>
#include <iomanip>
#include <cerrno>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include "cmd/site/nuking.hpp"
#include "cmd/error.hpp"
#include "db/nuking/nuking.hpp"
#include "text/factory.hpp"
#include "util/string.hpp"
#include "text/error.hpp"
#include "cmd/util.hpp"
#include "fs/path.hpp"
#include "acl/user.hpp"
#include "util/path/extattr.hpp"
#include "logs/logs.hpp"
#include "db/stats/stats.hpp"
#include "cfg/get.hpp"
#include "acl/path.hpp"
#include "cmd/arguments.hpp"
#include "util/path/status.hpp"
#include "util/path/dircontainer.hpp"
#include "util/path/recursivediriterator.hpp"
#include "fs/owner.hpp"
#include "fs/file.hpp"
#include "fs/directory.hpp"

namespace cmd { namespace site
{

namespace
{
const char* nukeIdAttributeName = "user.ebftpd.nukeid";

std::string GetNukeID(const fs::RealPath& path)
{ 
  char buf[25];
  int len = getxattr(path.CString(), nukeIdAttributeName, buf, sizeof(buf));
  if (len < 0)
  {
    if (errno != ENODATA && errno != ENOATTR && errno != ENOENT)
    {
      logs::Error("Error while reading filesystem attribute %1%: %2%: %3%", 
                  nukeIdAttributeName, path, util::Error::Failure(errno).Message());
    }
    return "";
  }
  
  buf[len] = '\0';
  return buf;
}

void SetNukeID(const fs::RealPath& path, const std::string& id)
{
  if (setxattr(path.CString(), nukeIdAttributeName, id.c_str(), id.length(), 0) < 0)
  {
    logs::Error("Error while writing filesystem attribute %1%: %2%: %3%", 
                nukeIdAttributeName, path, util::Error::Failure(errno).Message());
  }
}

void RemoveNukeID(const fs::RealPath& path)
{
  if (removexattr(path.CString(), nukeIdAttributeName) > 0 &&
      errno != ENOENT && errno != ENODATA && errno != ENOATTR)
  {
    logs::Error("Error while removing filesystem attribute %1%: %2%: %3%", 
                nukeIdAttributeName, path, util::Error::Failure(errno).Message());
  }
}

fs::RealPath NukedPath(const fs::RealPath& path)
{
  const auto& config = cfg::Get();
  std::string nukedName(boost::replace_all_copy(config.NukedirStyle().Format(), "%D", 
                                                path.Basename().ToString()));
  return path / nukedName;
}

}

void RegisterHeadFoot(text::TemplateSection& ts, const db::nuking::Nuke& nuke, 
                      const fs::VirtualPath& path)
{
  ts.RegisterValue("path", path.ToString());
  ts.RegisterValue("directory", path.Basename().ToString());
  ts.RegisterValue("multiplier", std::to_string(nuke.Multiplier()) + 
                   (nuke.IsPercent() ? "%" : ""));
  ts.RegisterValue("reason", nuke.Reason());
  ts.RegisterValue("section", nuke.Section().empty() ? 
                   "NoSection" : nuke.Section());
}

db::nuking::Nuke Nuke(const fs::VirtualPath& path, int multiplier, bool isPercent, 
                      const std::string& reason)
{
  class DoNuke
  {
    struct Nukee
    {
      
      long long kBytes;
      int files;
      long long credits;
      
      Nukee() : kBytes(0), files(0), credits(0) { }
    };
    
    const cfg::Config& config;
    fs::VirtualPath path;
    fs::RealPath real;
    int multiplier;
    bool isPercent;
    std::string reason;
    time_t modTime;
    long long totalKBytes;
    std::map<acl::UserID, Nukee> nukees;
    boost::optional<const cfg::Section&> section;
    boost::optional<db::nuking::Nuke> nuke;
    
    void CalculateNukees()
    {
      using namespace util::path;
      try
      {
         modTime = Status(path.ToString()).ModTime();
        
        for (const std::string& entry : RecursiveDirContainer(real.ToString(), true))
        {
          if (Basename(entry).front() == '.') continue;
          
          Status status(entry);
          if (!status.IsRegularFile()) continue;
          
          auto& nukee = nukees[fs::GetOwner(entry).UID()];
          long long kBytes = status.Size() / 1024;
          nukee.kBytes += kBytes;
          nukee.files++;
          totalKBytes += kBytes;
        }
      }
      catch (const util::SystemError& e)
      {
        logs::Error("Error while nuking %1%: %2%", path, e.what());
        throw e;
      }
    }
    
    void TakeCreditsEmpty(const std::string& sectionName)
    {
      if (!nukees.empty())
      {
        for (auto& kv : nukees)
        {
          auto user = acl::User::Load(kv.first);
          if (!user)
          {
            logs::Error("Unable to update user with uid %1% after nuke of: %2%", 
                        kv.first, path);
          }
          else
          {
            user->DecrSectionCreditsForce(sectionName, kv.second.credits);
          }
        }
      }
      else // otherwise we penalise the directory owner
      {
        auto owner = fs::GetOwner(real);
        auto& nukee = nukees[owner.UID()];
        nukee.credits = config.EmptyNuke();
        auto user = acl::User::Load(owner.UID());
        if (!user)
        {
          logs::Error("Unable to update user with uid %1% after nuke of: %2%", 
                      owner.UID(), path);
        }
        else
        {
          user->DecrSectionCreditsForce(sectionName, nukee.credits);
        }
      }
    }
    
    void TakeCreditsNotEmpty(const std::string& sectionName)
    {
      double percent = multiplier / 100.0;
      for (auto& kv : nukees)
      {
        auto user = acl::User::Load(kv.first);
        if (isPercent)
        {
          kv.second.credits = user->SectionCredits(sectionName) * percent;
          if (kv.second.credits < 0) kv.second.credits = 0;
        }
        else
        {
          kv.second.credits = kv.second.kBytes * multiplier;
        }

        if (!user)
        {
          logs::Error("Unable to update user with uid %1% after nuke of: %2%", 
                      kv.first, path);
        }
        else
        {
          user->DecrSectionCreditsForce(sectionName, kv.second.credits);
        }
      }
    }
    
    void TakeCredits()
    {
      std::string sectionName(section && section->SeparateCredits() ? section->Name() : "");
      if (totalKBytes < config.NukedirStyle().EmptyKBytes())
      {
        TakeCreditsEmpty(sectionName);
      }
      else
      {
        TakeCreditsNotEmpty(sectionName);
      }
    }
    
    void TakeStats()
    {
      std::string sectionName(section ? section->Name() : "");
      for (const auto& kv : nukees)
      {
        if (kv.second.kBytes > 0)
        {
          db::stats::UploadDecr(kv.first, kv.second.kBytes, modTime, 
                                sectionName, kv.second.files);
        }
      }
    }
    
    boost::optional<db::nuking::Nuke> LookupUnnuke()
    {
      boost::optional<db::nuking::Nuke> nuke;
      std::string id = GetNukeID(real);
      if (!id.empty()) nuke = db::nuking::LookupUnnukeByID(id);   
      if (!nuke) nuke = db::nuking::LookupUnnukeByPath(path.ToString());
      return nuke;
    }
    
    void UpdateDatabase()
    {
      std::vector<db::nuking::Nukee> nukees2;
      for (const auto& kv : nukees)
      {
        nukees2.emplace_back(kv.first, kv.second.kBytes, 
                             kv.second.files, kv.second.credits);
      }
      
      // remove old unnuke data if dir was unnuked in past
      auto unnuke = LookupUnnuke();
      if (unnuke) db::nuking::DelUnnuke(*unnuke);
      
      nuke = db::nuking::Nuke(path.ToString(), section ? section->Name() : "", 
                              reason, multiplier, isPercent, modTime, nukees2);
      db::nuking::AddNuke(*nuke);
    }

    void DeleteFiles()
    {
      for (const std::string& entry : util::path::RecursiveDirContainer(real.ToString(), true))
      {
        if (util::path::IsRegularFile(entry))
        {
          auto e = fs::DeleteFile(fs::RealPath(entry));
          if (!e)
          {
            logs::Error("Unable to delete nuked file: %1%", entry);
          }
        }
      }
    }
    
    void DeleteDirectories()
    {
      for (const std::string& entry : util::path::RecursiveDirContainer(real.ToString(), true))
      {
        if (util::path::IsDirectory(entry))
        {
          auto e = fs::RemoveDirectory(fs::RealPath(entry));
          if (!e)
          {
            logs::Error("Unable to delete nuked file: %1%", entry);
          }
        }
      }
    }
    
    void DeleteContents()
    {
      try
      {
        DeleteFiles();
        DeleteDirectories();
      }
      catch (const util::SystemError& e)
      {
        logs::Error("Unable to read nuked directory contents: %1%", path);
      }
    }
    
    void Delete()
    {
      auto e = fs::RemoveDirectory(real);
      if (!e)
      {
        logs::Error("Unable to remove nuked directory: %1%", path);
      }
    }
    
    void Rename()
    {
      assert(nuke);
      auto nukedPath = NukedPath(real);
      auto e = fs::RenameFile(real, nukedPath);
      if (!e)
      {
        logs::Error("Unable to rename nuked directory: %1% -> %2%: %3%", 
                    real, nukedPath, e.Message());
        SetNukeID(real, nuke->ID());
      }
      else
      {
        SetNukeID(nukedPath, nuke->ID());
      }
    }

    void ActionTheDirectory()
    {
      assert(nuke);
      auto action = config.NukedirStyle().GetAction();
      if (action != cfg::NukedirStyle::Keep)
      {
        DeleteContents();
      }

      if (action == cfg::NukedirStyle::DeleteAll)
      {
        Delete();
      }
      else
      {
        Rename();
      }
    }

  public:
    DoNuke(const fs::VirtualPath& path, int multiplier, bool isPercent, 
           const std::string& reason) :
      config(cfg::Get()),
      path(path),
      real(fs::MakeReal(path)),
      multiplier(multiplier),
      isPercent(isPercent),
      reason(reason),
      modTime(0),
      totalKBytes(0),
      section(config.SectionMatch(path.ToString(), true))
    { }
  
    db::nuking::Nuke operator()()
    {
      CalculateNukees();
      TakeCredits();
      TakeStats();
      UpdateDatabase();
      ActionTheDirectory();
      return *nuke;
    }
    
  } doNuke(path, multiplier, isPercent, reason);
  
  return doNuke();
}

void NUKECommand::Execute()
{
  auto args = cmd::ArgumentParser("path:s multi:s reason:m")(argStr);
  fs::VirtualPath path(fs::PathFromUser(args["path"]));
  
  const std::string& multi = args["multi"];
  const std::string& reason = args["reason"];
  
  bool isPercent = multi.back() == '%';
  int multiplier;
  try
  {
    const auto& config = cfg::Get();
    multiplier = util::StrToInt(multi.substr(0, multi.length() - isPercent));
    if ((config.MultiplierMax() != -1 && multiplier > config.MultiplierMax()) ||
        multiplier <= 0)
    {
      control.Reply(ftp::ActionNotOkay, "Invalid nuke multiplier / percent.");
      return;
    }    
  }
  catch (const std::bad_cast&)
  {
    throw cmd::SyntaxError();
  }
  
  if (!acl::path::DirAllowed<acl::path::Nuke>(client.User(), path))
  {
    throw cmd::PermissionError();
  }

  try
  {
    auto nuke = Nuke(path, multiplier, isPercent, reason);
    try
    {
      auto templ = text::Factory::GetTemplate("nuke");
    
      std::ostringstream os;
      RegisterHeadFoot(templ.Head(), nuke, path);
      RegisterHeadFoot(templ.Foot(), nuke, path);
      
      os << templ.Head().Compile();

      auto& body = templ.Body();
      unsigned index = 0;
      long long totalKBytes = 0;
      long long totalCredits = 0;
      int totalFiles = 0;

      for (const auto& nukee : nuke.Nukees())
      {
        body.RegisterValue("index", ++index);
        body.RegisterValue("username", acl::UIDToName(nukee.UID()));
        body.RegisterSize("size", nukee.KBytes());
        body.RegisterSize("credits", nukee.Credits());
        body.RegisterValue("files", nukee.Files());
        os << body.Compile();
        
        totalKBytes += nukee.KBytes();
        totalCredits += nukee.Credits();
        totalFiles += nukee.Files();
      }

      auto& foot = templ.Foot();
      foot.RegisterValue("total_nukees", nuke.Nukees().size());
      foot.RegisterSize("total_size", totalKBytes);
      foot.RegisterSize("total_credits", totalCredits);
      foot.RegisterValue("total_files", totalFiles);
      os << foot.Compile();
      
      control.Reply(ftp::CommandOkay, os.str());
    }
    catch (const text::TemplateError& e)
    {
      control.Reply(ftp::ActionNotOkay, e.Message());
    }
  }
  catch (const util::RuntimeError& e)
  {
    control.Format(ftp::ActionNotOkay, "Error while nuking: %1%", e.what());
  }
}

db::nuking::Nuke Unnuke(const fs::VirtualPath& path, const std::string& reason)
{
  class DoUnnuke
  {
    const cfg::Config& config;
    fs::VirtualPath path;
    fs::RealPath real;
    fs::RealPath nukedPath;
    std::string reason;
    db::nuking::Nuke nuke;
    boost::optional<const cfg::Section&> section;

    db::nuking::Nuke LookupNuke()
    {
      boost::optional<db::nuking::Nuke> nuke;
      std::string id = GetNukeID(nukedPath);
      if (!id.empty()) nuke = db::nuking::LookupNukeByID(id);   
      if (!nuke)
      {
        nuke = db::nuking::LookupNukeByPath(path.ToString());
        if (!nuke) throw util::RuntimeError("Unable to locate nuke data.");
      }
      return *nuke;      
    }
    
    bool SeparateCredits()
    {
      auto it = config.Sections().find(nuke.Section());
      return it != config.Sections().end() && it->second.SeparateCredits();
    }

    void RestoreStatsAndCredits()
    {
      std::string sectionName(SeparateCredits() ? nuke.Section() : "");
      for (const auto& nukee : nuke.Nukees())
      {
        auto user = acl::User::Load(nukee.UID());
        if (!user)
        {
          logs::Error("Unable to update user with uid %1% after unnuke of: %2%", 
                      nukee.UID(), path);
        }
        else
        {
          user->IncrSectionCredits(sectionName, nukee.Credits());
          db::stats::UploadIncr(nukee.UID(), nukee.KBytes(), nuke.ModTime(), 
                                nuke.Section(), nuke.Files());
        }
      }
    }

    void Rename()
    {
      RemoveNukeID(nukedPath);
      
      auto e = fs::RenameFile(nukedPath, real);
      if (!e && (!e.ValidErrno() || e.Errno() != ENOENT))
      {
        logs::Error("Unable to rename nuked directory: %1% -> %2%: %3%", 
                    nukedPath, real, util::Error::Failure(errno).Message());
      }
    }
    
    void UpdateDatabase()
    {
      db::nuking::DelNuke(nuke);
      nuke.Unnuke(reason);
      db::nuking::AddUnnuke(nuke);
      SetNukeID(real, nuke.ID());
    }
    
  public:
    DoUnnuke(const fs::VirtualPath& path, const std::string& reason) :
      config(cfg::Get()),
      path(path),
      real(fs::MakeReal(path)),
      nukedPath(NukedPath(real)),
      reason(reason),
      nuke(LookupNuke())
    { }

    db::nuking::Nuke operator()()
    {
      RestoreStatsAndCredits();
      Rename();
      UpdateDatabase();
      return nuke;
    }
    
  } doUnnuke(path, reason);
  
  return doUnnuke();
}

void UNNUKECommand::Execute()
{
  auto args = cmd::ArgumentParser("path:s reason:m")(argStr);
  fs::VirtualPath path(fs::PathFromUser(args["path"]));
  
  const std::string& reason = args["reason"];
  
  if (!acl::path::DirAllowed<acl::path::Nuke>(client.User(), path))
  {
    throw cmd::PermissionError();
  }

  try
  {
    auto nuke = Unnuke(path, reason);
    try
    {
      auto templ = text::Factory::GetTemplate("unnuke");
        
      std::ostringstream os;
      RegisterHeadFoot(templ.Head(), nuke, path);
      RegisterHeadFoot(templ.Foot(), nuke, path);
      
      os << templ.Head().Compile();

      auto& body = templ.Body();
      unsigned index = 0;
      long long totalKBytes = 0;
      long long totalCredits = 0;
      int totalFiles = 0;
      for (const auto& nukee : nuke.Nukees())
      {
        body.RegisterValue("index", ++index);
        body.RegisterValue("username", acl::UIDToName(nukee.UID()));
        body.RegisterSize("size", nukee.KBytes());
        body.RegisterSize("credits", nukee.Credits());
        body.RegisterValue("files", nukee.Files());
        os << body.Compile();
        
        totalKBytes += nukee.KBytes();
        totalCredits += nukee.Credits();
        totalFiles += nukee.Files();
      }

      auto& foot = templ.Foot();
      foot.RegisterValue("total_nukees", nuke.Nukees().size());
      foot.RegisterSize("total_size", totalKBytes);
      foot.RegisterSize("total_credits", totalCredits);
      foot.RegisterValue("total_files", totalFiles);
      os << foot.Compile();
      
      control.Reply(ftp::CommandOkay, os.str());
    }
    catch (const text::TemplateError& e)
    {
      control.Reply(ftp::ActionNotOkay, e.Message());
    }
  }
  catch (const util::RuntimeError& e)
  {
    control.Format(ftp::ActionNotOkay, "Error while unnuking: %1%", e.what());
  }
}

std::string FormatNukees(const std::vector<db::nuking::Nukee>& nukees)
{
  std::ostringstream os;
  for (const auto& nukee : nukees)
  {
    if (!os.str().empty()) os << ", ";
    os << acl::UIDToName(nukee.UID()) 
       << std::setprecision(2) << std::fixed << (nukee.KBytes() / 1024.0) << "MB";
  }
  return os.str();
}

void NUKESCommand::Execute()
{
  int number = 10;
  if (args.size() == 2)
  {
    try
    {
      number = boost::lexical_cast<int>(args[1]);
      if (number <= 0) throw boost::bad_lexical_cast();
    }
    catch (const boost::bad_lexical_cast&)
    {
      throw cmd::SyntaxError();
    }
  }

  auto nukes = isUnnukes ? db::nuking::NewestUnnukes(number) :
                           db::nuking::NewestNukes(number);
                           
  try
  {
    auto templ = text::Factory::GetTemplate(isUnnukes ? "unnukes" : "nukes");

    std::ostringstream os;
    os << templ.Head().Compile();

    auto& body = templ.Body();

    auto now = boost::posix_time::second_clock::local_time();
    unsigned index = 0;
    
    for (const auto& nuke : nukes)
    {
      body.RegisterValue("index", ++index);
      body.RegisterValue("datetime", boost::lexical_cast<std::string>(nuke.DateTime()));
      body.RegisterValue("modtime", boost::lexical_cast<std::string>(nuke.ModTime()));
      body.RegisterValue("age", Age(now - nuke.DateTime()));
      body.RegisterValue("files", nuke.Files());
      body.RegisterSize("size", nuke.KBytes());
      body.RegisterValue("directory", fs::Path(nuke.Path()).Basename().ToString());
      body.RegisterValue("path", nuke.Path());
      body.RegisterValue("section", nuke.Section());
      body.RegisterValue("nukees", FormatNukees(nuke.Nukees()));
      body.RegisterValue("reason", nuke.Reason());
      
      std::string multiplier(std::to_string(nuke.Multiplier()));
      if (nuke.IsPercent()) multiplier += '%';    
      else multiplier += 'x';
      
      body.RegisterValue("multi", multiplier);

      os << body.Compile();
    }

    auto& foot = templ.Foot();
    foot.RegisterValue("count", nukes.size());
    os << foot.Compile();
    
    control.Reply(ftp::CommandOkay, os.str());
  }
  catch (const text::TemplateError& e)
  {
    control.Reply(ftp::ActionNotOkay, e.Message());
  }
}

} /* site namespace */
} /* cmd namespace */
