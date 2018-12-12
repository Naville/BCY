#include "BCY/DownloadFilter.hpp"
#include "BCY/Utils.hpp"
#include <algorithm>
#include <fstream>
#include <regex>
#include <iostream>
#include <boost/log/trivial.hpp>
using namespace std;
using namespace SQLite;
using namespace nlohmann;
namespace BCY {
    DownloadFilter::DownloadFilter(string Path) {
        DBPath=Path;
        Database DB(DBPath,SQLite::OPEN_CREATE||OPEN_READWRITE);
        Statement Q(DB, "SELECT Value FROM PyBCY WHERE Key=\"BCYDownloadFilter\"");
        Q.executeStep();
        if (Q.hasRow()) {
            json j = json::parse(Q.getColumn(0).getString());
            loadRulesFromJSON(j);
        }
    }
    DownloadFilter::~DownloadFilter() {
        json j;
        j["UID"] = UIDList;
        j["Work"] = WorkList;
        j["Tag"] = TagList;
        j["UserName"] = UserNameList;
        j["Type"] = TypeList;
        Database DB(DBPath,OPEN_READWRITE);
        Statement Q(DB, "INSERT OR REPLACE INTO PyBCY (Key,Value) VALUES(?,?)");
        Q.bind(1, "BCYDownloadFilter");
        Q.bind(2, j.dump());
        Q.executeStep();
    }
    bool DownloadFilter::shouldBlock(json abstract) {
        string item_id="null";
        if(abstract.find("item_id")!=abstract.end()){
            item_id=ensure_string(abstract["item_id"]);
        }
        
        if (find(UIDList.begin(), UIDList.end(), abstract["uid"]) != UIDList.end()) {
            BOOST_LOG_TRIVIAL(debug)<<item_id<<" Blocked By UID Rule:"<<abstract["uid"]<<endl;
            return true;
        }
        if (abstract.find("post_tags") != abstract.end()) {
            for (json &j : abstract["post_tags"]) {
                string tagName = j["tag_name"];
                if (find(TagList.begin(), TagList.end(), tagName) != TagList.end()) {
                    BOOST_LOG_TRIVIAL(debug)<<item_id<<" Blocked By Tag Rule:"<<tagName<<endl;
                    return true;
                }
            }
        }
        if (abstract.find("work") != abstract.end()) {
            for (string name : WorkList) {
                smatch match;
                string foo = abstract["work"];
                if (regex_search(foo, match, regex(name)) && match.size() >= 1) {
                    BOOST_LOG_TRIVIAL(debug)<<item_id<<" Blocked By Regex Work Rule:"<<name<<endl;
                    return true;
                }
            }
        }
        for (string name : UserNameList) {
            smatch match;
            string foo = abstract["profile"]["uname"];
            if (regex_search(foo, match, regex(name)) && match.size() >= 1) {
                BOOST_LOG_TRIVIAL(debug)<<item_id<<" Blocked By Regex UserName Rule:"<<name<<endl;
                return true;
            }
        }
        if (find(TypeList.begin(), TypeList.end(), abstract["type"]) !=
            TypeList.end()) {
            BOOST_LOG_TRIVIAL(debug)<<item_id<<" Blocked Due to its type:"<<ensure_string(abstract["type"])<<endl;
            return true;
        }
        return false;
    }
    void DownloadFilter::loadRulesFromJSON(json rules) {
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
