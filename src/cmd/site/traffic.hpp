#ifndef __CMD_SITE_TRAFFIC_HPP
#define __CMD_SITE_TRAFFIC_HPP

#include <string>
#include "cmd/command.hpp"

namespace cmd { namespace site
{

class TRAFFICCommand : public Command
{
public:
  TRAFFICCommand(ftp::Client& client, const std::string& argStr, const Args& args) :
    Command(client, client.Control(), client.Data(), argStr, args) { }

  void Execute();
};

} /* site namespace */
} /* cmd namespace */

#endif
