#include <BCY/Base64.h>
#include <BCY/DownloadFilter.hpp>
#include <BCY/Utils.hpp>
#include <algorithm>
#include <boost/log/trivial.hpp>
#include <chaiscript/chaiscript.hpp>
#include <fstream>
#include <iostream>
#include <regex>
using namespace std;
using namespace SQLite;
using namespace chaiscript;
namespace BCY {
DownloadFilter::DownloadFilter(string Path) {
  DBPath = Path;
  Database DB(DBPath, SQLite::OPEN_CREATE || OPEN_READWRITE);
  Statement Q(DB, "SELECT Value FROM PyBCY WHERE Key=\"BCYDownloadFilter\"");
  Q.executeStep();
  if (Q.hasRow()) {
    web::json::value j = web::json::value::parse(Q.getColumn(0).getString());
    loadRulesFromJSON(j);
  }
}
DownloadFilter::~DownloadFilter() {
  web::json::value j;
  vector<web::json::value> tmp;
  const std::array<string, 5> names = {"UID", "Tag", "UserName", "Items",
                                       "ScriptList"};
  const std::array<vector<string>, 5> elements = {
      UIDList, TagList, UserNameList, ItemList, ScriptList};
  static_assert(names.size() == elements.size(), "Array Size Mismatch");
  for (decltype(names.size()) i = 0; i < names.size(); i++) {
    vector<string> element = elements[i];
    vector<web::json::value> tmps;
    string key = names[i];
    for (string foo : element) {
      string processed;
      if (key.find("Script") != string::npos) {
        Base64::Encode(foo, &processed);
      } else {
        processed = foo;
      }
      tmps.push_back(web::json::value(processed));
    }
    j[key] = web::json::value::array(tmps);
  }
  Database DB(DBPath, OPEN_READWRITE);
  Statement Q(DB, "INSERT OR REPLACE INTO PyBCY (Key,Value) VALUES(?,?)");
  Q.bind(1, "BCYDownloadFilter");
  Q.bind(2, j.serialize());
  Q.executeStep();
}
bool DownloadFilter::shouldBlockItem(DownloadUtils::Info &Inf) {
  string uid = std::get<0>(Inf);
  string item_id = std::get<1>(Inf);
  string title = std::get<2>(Inf);
  vector<string> tags = std::get<3>(Inf);
  struct std::tm tm;
  std::istringstream ss(std::get<4>(Inf));
  vector<web::json::value> multis;
  for(web::json::value v:std::get<6>(Inf)){
    multis.push_back(v);
  }
  web::json::value multi=web::json::value::array(multis);
  ss >> std::get_time(&tm, "%H:%M:%S"); // or just %T in this case
  std::time_t ctime = mktime(&tm);
  string desc = std::get<5>(Inf);
  if (find(UIDList.begin(), UIDList.end(), uid) != UIDList.end()) {
    BOOST_LOG_TRIVIAL(debug) << item_id << " blocked by uid rules" << endl;
    return true;
  }
  if (find(ItemList.begin(), ItemList.end(), item_id) != ItemList.end()) {
    BOOST_LOG_TRIVIAL(debug) << item_id << " blocked by item_id rules" << endl;
    return true;
  }
  for (string tag : tags) {
    if (find(TagList.begin(), TagList.end(), tag) != TagList.end()) {
      BOOST_LOG_TRIVIAL(debug) << item_id << " blocked by tag rules" << endl;
      return true;
    }
  }
  for (BCYFilterHandler handle : filterHandlers) {
    if (handle(Inf) < 0) {
      BOOST_LOG_TRIVIAL(debug)
          << item_id << " blocked by custom filter handler" << endl;
      return true;
    }
  }
  // Scripting
  if (ScriptList.size() > 0) {
    ChaiScript chai;
    chai.add(chaiscript::const_var(uid), "uid");
    chai.add(chaiscript::const_var(item_id), "item_id");
    chai.add(chaiscript::const_var(title), "title");
    chai.add(chaiscript::const_var(tags), "tags");
    chai.add(chaiscript::const_var(ctime), "ctime");
    chai.add(chaiscript::const_var(desc), "desc");
    chai.add(chaiscript::const_var(multi.serialize()), "multi");
    for (std::string script : ScriptList) {
      int res = chai.eval<int>(script);
      if (res > 0) {
        return false;
      } else if (res < 0) {
        BOOST_LOG_TRIVIAL(debug)
            << item_id << " blocked by script: "<<chai.boxed_cast<string>(chai.get_locals()["ScriptName"])  << endl;
        return true;
      } else {
        continue;
      }
    }
  }

  return false;
}
void DownloadFilter::addFilterHandler(BCYFilterHandler handle) {
  filterHandlers.push_back(handle);
}
bool DownloadFilter::shouldBlockAbstract(web::json::value &Inf) {

  if (Inf.has_field("item_detail")) {
    Inf = Inf["item_detail"];
  }
  string ctime = ensure_string(Inf["ctime"]);
  string uid = ensure_string(Inf["uid"]);
  string item_id = ensure_string(Inf["item_id"]);
  string desc = ensure_string(Inf["plain"]);
  vector<string> tags;
  for (web::json::value foo : Inf["post_tags"].as_array()) {
    tags.push_back(foo["tag_name"].as_string());
  }
  // AbstractInfo's multi is incomplete
  // Use this as a placeholder.
  // Our second pass with Detail will execute those multi-based filter rules
  web::json::array multi = web::json::value::array().as_array();
  auto tup = std::tuple<std::string /*UID*/, std::string /*item_id*/,
                        std::string /*Title*/, vector<string> /*Tags*/,
                        std::string /*ctime*/, std::string /*Description*/,
                        web::json::array /*multi*/, std::string /*videoID*/>(
      uid, item_id, "", tags, ctime, desc, multi, "");
  return shouldBlockItem(tup);
}
void DownloadFilter::loadRulesFromJSON(web::json::value rules) {
  if (rules.has_field("UID")) {
    auto bar = rules["UID"].as_array();
    for (auto item : bar) {
      UIDList.push_back(item.as_string());
    }
  }
  if (rules.has_field("Items")) {
    auto foo = rules["Items"].as_array();
    for (auto item : foo) {
      ItemList.push_back(item.as_string());
    }
  }
  if (rules.has_field("Tag")) {
    auto foo = rules["Tag"].as_array();
    for (auto item : foo) {
      TagList.push_back(item.as_string());
    }
  }
  if (rules.has_field("UserName")) {
    auto foo = rules["UserName"].as_array();
    for (auto item : foo) {
      UserNameList.push_back(item.as_string());
    }
  }
  if (rules.has_field("ScriptList")) {
    auto foo = rules["ScriptList"].as_array();
    for (auto item : foo) {
      string SRC = item.as_string();
      string dec;
      Base64::Decode(SRC, &dec);
      ScriptList.push_back(dec);
    }
  }
}
} // namespace BCY
