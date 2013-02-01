#include <sstream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/trim.hpp>
#include "cmd/site/new.hpp"
#include "db/index/index.hpp"
#include "text/error.hpp"
#include "text/factory.hpp"
#include "fs/directory.hpp"
#include "cfg/get.hpp"
#include "util/timepair.hpp"
#include "cmd/error.hpp"

namespace cmd { namespace site
{

namespace
{

std::string Age(boost::posix_time::time_duration age)
{
  namespace pt = boost::posix_time;
  
  int days = age.hours() / 24;
  age -= pt::hours(days * 24);
  
  int fields = 0;
  if (days > 99) return boost::lexical_cast<std::string>(days) + "d";
  
  std::ostringstream os;
  if (days > 0)
  {
    os << std::setw(2) << days << "d ";
    ++fields;
  }
  
  if (age.hours() > 0)
  {
    os << std::setw(2) << age.hours() << "h ";
    if (++fields >= 2) return os.str();
  }
  
  if (age.minutes() > 0)
  {
    os << std::setw(2) << age.minutes() << "m ";
    if (++fields >= 2) return os.str();
  }
  
  os << std::setw(2) << age.seconds() << "s ";
  return boost::trim_copy(os.str());
}

}

void NEWCommand::Execute()
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

  auto results = db::index::Newest(number);

  boost::optional<text::Template> templ;
  try
  {
    templ.reset(text::Factory::GetTemplate("new"));
  }
  catch (const text::TemplateError& e)
  {
    control.Reply(ftp::ActionNotOkay, e.Message());
    return;
  }
  
  std::ostringstream os;
  os << templ->Head().Compile();

  text::TemplateSection& body = templ->Body();

  auto now = boost::posix_time::second_clock::local_time();
  unsigned index = 0;
  for (const auto& result : results)
  {
    long long kBytes;
    std::cout << fs::MakeReal(fs::VirtualPath(result.path)) << std::endl;
    auto e = fs::DirectorySize(fs::MakeReal(fs::VirtualPath(result.path)),
                               cfg::Get().DirSizeDepth(), kBytes);
    if (e.Errno() == ENOENT)
    {
      db::index::Delete(result.path);
      continue;
    }
    
    body.RegisterValue("index", ++index);
    body.RegisterValue("datetime", boost::lexical_cast<std::string>(result.dateTime));
    body.RegisterValue("age", Age(now - result.dateTime));
    body.RegisterValue("path", fs::Path(result.path).Basename().ToString());
    body.RegisterValue("section", result.section);
    body.RegisterSize("size", e ? kBytes : -1);
    os << body.Compile();
  }

  text::TemplateSection& foot = templ->Foot();
  foot.RegisterValue("count", results.size());
  os << foot.Compile();
  
  control.Reply(ftp::CommandOkay, os.str());
}

} /* site namespace */
} /* cmd namespace */
