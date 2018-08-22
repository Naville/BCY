#include "DownloadFilter.hpp"
#include <algorithm>
#include <regex>
#include <fstream>
using namespace std;
using namespace SQLite;
namespace BCY {
BCYDownloadFilter::BCYDownloadFilter(Database *db) {
  DB = db;
  Statement Q(*DB, "SELECT Value FROM PyBCY WHERE Key=\"BCYDownloadFilter\"");
  Q.executeStep();
  if (Q.hasRow()) {
    json j = json::parse(Q.getColumn(0).getString());
    loadRulesFromJSON(j);
  }
}
BCYDownloadFilter::~BCYDownloadFilter() {
  json j;
  j["UID"] = UIDList;
  j["Work"] = WorkList;
  j["Tag"] = TagList;
  j["UserName"] = UserNameList;
  j["Type"] = TypeList;
  Statement Q(*DB, "INSERT OR REPLACE INTO PyBCY (Key,Value) VALUES(?,?)");
  Q.bind(1, "BCYDownloadFilter");
  Q.bind(2, j.dump());
  Q.executeStep();
}
bool BCYDownloadFilter::shouldBlock(json abstract) {
  if (find(UIDList.begin(), UIDList.end(), abstract["uid"]) != UIDList.end()) {
    return true;
  }
  if (abstract.find("post_tags") != abstract.end()) {
    for (json &j : abstract["post_tags"]) {
      string tagName = j["tag_name"];
      if (find(TagList.begin(), TagList.end(), tagName) != TagList.end()) {
        return true;
      }
    }
  }
  if (abstract.find("work") != abstract.end()) {
    for (string name : WorkList) {
      smatch match;
      string foo = abstract["work"];
      if (regex_search(foo, match, regex(name)) && match.size() >= 1) {
        return true;
      }
    }
  }
  for (string name : UserNameList) {
    smatch match;
    string foo = abstract["profile"]["uname"];
    if (regex_search(foo, match, regex(name)) && match.size() >= 1) {
      return true;
    }
  }
  if (find(TypeList.begin(), TypeList.end(), abstract["type"]) !=
      TypeList.end()) {
    return true;
  }
  return false;
}
void BCYDownloadFilter::loadRulesFromJSON(json rules) {
  if (rules.find("UID") != rules.end()) {
    vector<string> foo = rules["UID"];
    UIDList.insert(UIDList.end(), foo.begin(), foo.end());
  }
  if (rules.find("Work") != rules.end()) {
    vector<string> foo = rules["Work"];
    WorkList.insert(WorkList.end(), foo.begin(), foo.end());
  }
  if (rules.find("Tag") != rules.end()) {
    vector<string> foo = rules["Tag"];
    TagList.insert(TagList.end(), foo.begin(), foo.end());
  }
  if (rules.find("UserName") != rules.end()) {
    vector<string> foo = rules["UserName"];
    UserNameList.insert(UserNameList.end(), foo.begin(), foo.end());
  }
  if (rules.find("Type") != rules.end()) {
    vector<string> foo = rules["Type"];
    TypeList.insert(TypeList.end(), foo.begin(), foo.end());
  }
}
} // namespace BCY
