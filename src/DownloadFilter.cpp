#include "BCY/DownloadFilter.hpp"
#include "BCY/Utils.hpp"
#include <algorithm>
#include <boost/log/trivial.hpp>
#include <fstream>
#include <iostream>
#include <regex>
using namespace std;
using namespace SQLite;
using json = web::json::value;
namespace BCY {
DownloadFilter::DownloadFilter(string Path) {
  DBPath = Path;
  Database DB(DBPath, SQLite::OPEN_CREATE || OPEN_READWRITE);
  Statement Q(DB, "SELECT Value FROM PyBCY WHERE Key=\"BCYDownloadFilter\"");
  Q.executeStep();
  if (Q.hasRow()) {
    json j = json::parse(Q.getColumn(0).getString());
    loadRulesFromJSON(j);
  }
}
DownloadFilter::~DownloadFilter() {
  json j;
  j["UID"] = web::json::value::array(UIDList);
  j["Work"] = web::json::value::array(WorkList);
  j["Tag"] = web::json::value::array(TagList);
  j["UserName"] = web::json::value::array(UserNameList);
  j["Type"] = web::json::value::array(TypeList);
  Database DB(DBPath, OPEN_READWRITE);
  Statement Q(DB, "INSERT OR REPLACE INTO PyBCY (Key,Value) VALUES(?,?)");
  Q.bind(1, "BCYDownloadFilter");
  Q.bind(2, j.serialize());
  Q.executeStep();
}
bool DownloadFilter::shouldBlockDetail(json abstract) {
  string item_id = "null";
  if (abstract.has_field("item_id")) {
    item_id = ensure_string(abstract["item_id"]);
  }
  for (auto bar : UserNameList) {
    string name = bar.as_string();
    smatch match;
    string foo = abstract["profile"]["uname"].as_string();
    try {
      if (regex_search(foo, match, regex(name)) && match.size() >= 1) {
        BOOST_LOG_TRIVIAL(debug)
            << item_id << " Blocked By Regex UserName Rule:" << name << endl;
        return true;
      }
    } catch (const std::regex_error &e) {
      BOOST_LOG_TRIVIAL(error)
          << "UserName Regex Filter Error:" << e.what() << endl;
    }
  }
  return false;
}
bool DownloadFilter::shouldBlockAbstract(json abstract) {
  string item_id = "null";
  if (abstract.has_field("item_id")) {
    item_id = ensure_string(abstract["item_id"]);
  }

  if (find(UIDList.begin(), UIDList.end(), abstract["uid"]) != UIDList.end()) {
    BOOST_LOG_TRIVIAL(debug)
        << item_id << " Blocked By UID Rule:" << abstract["uid"] << endl;
    return true;
  }
  if (abstract.has_field("post_tags")) {
    for (json &j : abstract["post_tags"].as_array()) {
      string tagName = j["tag_name"].as_string();
      for (json j : TagList) {
        if (j.as_string() == tagName) {

          BOOST_LOG_TRIVIAL(debug)
              << item_id << " Blocked By Tag Rule:" << tagName << endl;
          return true;
        }
      }
    }
  }
  if (abstract.has_field("work")) {
    for (auto bar : WorkList) {
      string name = bar.as_string();
      smatch match;
      string foo = abstract["work"].as_string();
      if (regex_search(foo, match, regex(name)) && match.size() >= 1) {
        BOOST_LOG_TRIVIAL(debug)
            << item_id << " Blocked By Regex Work Rule:" << name << endl;
        return true;
      }
    }
  }
  if (find(TypeList.begin(), TypeList.end(), abstract["type"]) !=
      TypeList.end()) {
    BOOST_LOG_TRIVIAL(debug) << item_id << " Blocked Due to its type:"
                             << ensure_string(abstract["type"]) << endl;
    return true;
  }
  return false;
}
void DownloadFilter::loadRulesFromJSON(json rules) {
  if (rules.has_field("UID")) {
    auto bar = rules["UID"].as_array();
    for (auto item : bar) {
      UIDList.push_back(item);
    }
  }
  if (rules.has_field("Work")) {
    auto foo = rules["Work"].as_array();
    for (auto item : foo) {
      WorkList.push_back(item);
    }
  }
  if (rules.has_field("Tag")) {
    auto foo = rules["Tag"].as_array();
    for (auto item : foo) {
      TagList.push_back(item);
    }
  }
  if (rules.has_field("UserName")) {
    auto foo = rules["UserName"].as_array();
    for (auto item : foo) {
      UserNameList.push_back(item);
    }
  }
  if (rules.has_field("Type")) {
    auto foo = rules["Type"].as_array();
    for (auto item : foo) {
      TypeList.push_back(item);
    }
  }
}
} // namespace BCY
