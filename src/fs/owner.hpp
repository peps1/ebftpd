#ifndef __FS_OWNER_HPP
#define __FS_OWNER_HPP

#include <ostream>
#include "acl/types.hpp"
#include "fs/path.hpp"

namespace util
{
class Error;
}

namespace fs
{

class Owner
{
  acl::UserID uid;
  acl::GroupID gid;
  
public:
  Owner(acl::UserID uid, acl::GroupID gid) : uid(uid), gid(gid) { }
  
  acl::UserID UID() const { return uid; }
  acl::GroupID GID() const { return gid; }
};

Owner GetOwner(const std::string& path);
util::Error SetOwner(const std::string& path, const Owner& owner);

Owner GetOwner(const RealPath& path);
util::Error SetOwner(const RealPath& path, const Owner& owner);

inline std::ostream& operator<<(std::ostream& os, const Owner& owner)
{
  os << owner.UID() << "," << owner.GID();
  return os;
}

} /* fs namespace */



#endif
