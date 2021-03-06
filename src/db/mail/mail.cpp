#include <boost/optional.hpp>
#include "db/mail/mail.hpp"
#include "db/mail/message.hpp"
#include "db/connection.hpp"

namespace db
{

template <> mongo::BSONObj Serialize<mail::Message>(const mail::Message& message)
{
  mongo::BSONObjBuilder bob;
  bob.append("recipient", message.recipient);
  bob.append("sender", message.sender);
  bob.append("time sent", ToDateT(message.timeSent));
  bob.append("body", message.body);
  bob.append("status", util::EnumToString(message.status));
  return bob.obj();
}

template <> mail::Message Unserialize<mail::Message>(const mongo::BSONObj& obj)
{
  mail::Message message;
  message.recipient = obj["recipient"].Int();
  message.sender = obj["sender"].String();
  message.timeSent = ToPosixTime(obj["time sent"].Date());
  message.body = obj["body"].String();
  message.status = util::EnumFromString<db::mail
      ::Status>(obj["status"].String());
  mongo::BSONElement oid;
  obj.getObjectID(oid);
  message.oid = oid.OID();
  return message;
}

namespace mail
{

void Send(const Message& message)
{
  NoErrorConnection conn;
  conn.Insert("mail", db::Serialize<mail::Message>(message));
}

std::vector<Message> Get(acl::UserID recipient)
{
  NoErrorConnection conn;
  return conn.QueryMulti<Message>("mail", QUERY("recipient" << recipient));
}

boost::optional<mongo::OID> IndexToOID(acl::UserID recipient, int index)
{
  NoErrorConnection conn;
  mongo::Query query = QUERY("recipient" << recipient);
  auto results = conn.Query("mail", query, 1, index);
  if (results.empty()) return boost::none;
  
  mongo::BSONElement oidElem;
  results.front().getObjectID(oidElem);
  return boost::optional<mongo::OID>(oidElem.OID());
}

bool Save(acl::UserID recipient, int index)
{
  auto oid = IndexToOID(recipient, index);
  if (!oid) return false;
  
  NoErrorConnection conn;
  mongo::Query query = QUERY("_id" << *oid);
  return conn.Update("mail", query, BSON("$set" << BSON("status" << "saved"))) > 0;
}

int SaveTrash(acl::UserID recipient)
{
  mongo::Query query = QUERY("recipient" << recipient << "status" << "trash");
  NoErrorConnection conn;
  return conn.Update("mail", query, BSON("$set" << BSON("status" << "saved")));
}

bool Purge(acl::UserID recipient, int index)
{
  auto oid = IndexToOID(recipient, index);
  if (!oid) return false;

  NoErrorConnection conn;
  return conn.Remove("mail", QUERY("_id" << *oid)) > 0;
}

int PurgeTrash(acl::UserID recipient)
{
  mongo::Query query = QUERY("recipient" << recipient << "status" << "trash");
  NoErrorConnection conn;
  return conn.Remove("mail", query);
}

void LogOffPurgeTrash(acl::UserID recipient)
{
  PurgeTrash(recipient);
}

void Trash(const Message& message)
{
  mongo::Query query = QUERY("_id" << message.oid);
  NoErrorConnection conn;
  conn.Update("mail", query, BSON("$set" << BSON("status" << "trash")));
}

} /* mail namespace */
} /* db namespace */
