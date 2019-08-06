#include <BCY/DownloadUtils.hpp>
#include <BCY/DownloadFilter.hpp>
#include <BCY/Base64.h>
#include <BCY/Utils.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lockfree/stack.hpp>
#include <boost/log/trivial.hpp>
#include <boost/thread.hpp>
#include <cpprest/http_client.h>
#include <execinfo.h>
#include <fstream>
#include <regex>
#ifdef __APPLE__
#define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
#endif
#include <boost/exception/all.hpp>
#include <boost/stacktrace.hpp>
using namespace CryptoPP;
namespace fs = boost::filesystem;
using namespace boost::asio;
using namespace std;
using namespace SQLite;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace BCY {
DownloadUtils::DownloadUtils(string PathBase, int queryThreadCount,
                             int downloadThreadCount, string Path) {

  saveRoot = PathBase;
  fs::path dir(PathBase);
  fs::path file("BCYInfo.db");
  fs::path full_path = dir / file;
  if (Path != "") {
    DBPath = Path;
  } else {
    DBPath = full_path.string();
  }
  Database DB(DBPath, SQLite::OPEN_READWRITE | OPEN_CREATE);
  if (queryThreadCount == -1) {
    queryThreadCount = std::thread::hardware_concurrency() / 2;
  }
  if (downloadThreadCount == -1) {
    downloadThreadCount = std::thread::hardware_concurrency() / 2;
  }

  queryThread = new thread_pool(queryThreadCount);
  downloadThread = new thread_pool(downloadThreadCount);
  std::lock_guard<mutex> guard(dbLock);
  DB.exec("CREATE TABLE IF NOT EXISTS UserInfo (uid INTEGER,uname "
          "STRING,self_intro STRING,avatar STRING,isValueUser INTEGER,Tags "
          "STRING NOT NULL DEFAULT '[]',UNIQUE(uid) ON CONFLICT IGNORE)");
  DB.exec("CREATE TABLE IF NOT EXISTS EventInfo (event_id INTEGER,etime "
          "INTEGER,stime INTEGER,cover STRING,intro STRING,Info "
          "STRING,UNIQUE(event_id) ON CONFLICT IGNORE)");
  DB.exec("CREATE TABLE IF NOT EXISTS GroupInfo (gid INTEGER,GroupName "
          "STRING,UNIQUE(gid) ON CONFLICT IGNORE)");
  DB.exec("CREATE TABLE IF NOT EXISTS ItemInfo (uid INTEGER DEFAULT 0,item_id "
          "INTEGER DEFAULT 0,Title STRING NOT NULL DEFAULT '',Tags STRING NOT "
          "NULL DEFAULT '[]',ctime INTEGER DEFAULT 0,Description STRING NOT "
          "NULL DEFAULT '',Images STRING NOT NULL DEFAULT '[]',VideoID STRING "
          "NOT NULL DEFAULT '',UNIQUE (item_id) ON CONFLICT REPLACE)");
  DB.exec("CREATE TABLE IF NOT EXISTS PyBCY (Key STRING DEFAULT '',Value "
          "STRING NOT NULL DEFAULT '',UNIQUE(Key) ON CONFLICT IGNORE)");
  DB.exec("PRAGMA journal_mode=WAL;");
  filter = new DownloadFilter(DBPath);
  // Create Download Temp First
  boost::system::error_code ec;
  fs::path TempPath = fs::path(PathBase) / fs::path("DownloadTemp");
  fs::create_directories(TempPath, ec);
#warning Remove this after scripting support landed

  filter->addFilterHandler([](DownloadUtils::Info& Inf){
      std::vector<std::string> tags = std::get<3>(Inf);
    if( find(tags.begin(),tags.end(),"COS")!=tags.end()){
        return 0;
    }
    else{
        array<string,4> drawKWs={"动画","漫画","绘画","手绘"};
        for(string ele:drawKWs){
            if( find(tags.begin(),tags.end(),ele)!=tags.end()){
                return -1;
            }
        }
        return 0;
    }
  });
}
void DownloadUtils::downloadFromAbstractInfo(web::json::value& AbstractInfo) {
  downloadFromAbstractInfo(AbstractInfo, this->enableFilter);
}
void DownloadUtils::downloadFromAbstractInfo(web::json::value& AbstractInfo,
                                             bool runFilter) {
  if (stop) {
    return;
  }
#ifndef DEBUG
  boost::asio::post(*queryThread, [=]() {
 #endif
    if (stop) {
      return;
    }
    boost::this_thread::interruption_point();
#ifndef DEBUG
    try {
#endif
      if (runFilter == false || filter->shouldBlockAbstract(const_cast<web::json::value&>(AbstractInfo))==false
          ) {
          string item_id =
              ensure_string(AbstractInfo.at("item_id"));

        boost::this_thread::interruption_point();
              optional<DownloadUtils::Info> detail= loadInfo(item_id);
        boost::this_thread::interruption_point();
        if (detail.has_value()==false) {
          boost::this_thread::interruption_point();
          detail.emplace(canonicalizeRawServerDetail(core.item_detail(item_id)["data"]));
          boost::this_thread::interruption_point();
          try {
            downloadFromInfo(*detail,runFilter);
          } catch (boost::thread_interrupted) {
            BOOST_LOG_TRIVIAL(debug)
                << "Cancelling Thread:" << boost::this_thread::get_id() << endl;
            return;
          }
        } else {
          try {
            downloadFromInfo(
                *detail,
                runFilter);
          } catch (boost::thread_interrupted) {
            BOOST_LOG_TRIVIAL(debug)
                << "Cancelling Thread:" << boost::this_thread::get_id() << endl;
            return;
          }
        }
      }
#ifndef DEBUG
    }
    catch (exception &exp) {
      BOOST_LOG_TRIVIAL(error)
          << exp.what() << __FILE__ << ":" << __LINE__ << endl
          << AbstractInfo.serialize() << endl;
      std::cerr << boost::stacktrace::stacktrace() << '\n';
    }
#endif
#ifndef DEBUG
  });
#endif

}
web::json::value DownloadUtils::saveOrLoadUser(string uid, string uname,
                                               string intro, string avatar,
                                               bool isValueUser) {
  std::lock_guard<mutex> guard(dbLock);
  Database DB(DBPath, SQLite::OPEN_READWRITE);
  Statement Q(DB, "SELECT uid,uname,self_intro,avatar,isValueUser,Tags FROM "
                  "UserInfo WHERE uid=?");
  Q.bind(1, uid);
  Q.executeStep();
  if (Q.hasRow()) {
    web::json::value j;
    j["uid"] = web::json::value::string(Q.getColumn(0).getString());
    j["uname"] = web::json::value::string(Q.getColumn(1).getString());
    j["intro"] = web::json::value::string(Q.getColumn(2).getString());
    j["avatar"] = web::json::value::string(Q.getColumn(3).getString());
    int isV = Q.getColumn(4).getInt();
    j["isValueUser"] = web::json::value::boolean((isV != 0));
    j["Tags"] = web::json::value::parse(Q.getColumn(5).getString());
    return j;
  } else {
    Statement Q(
        DB,
        "INSERT INTO UserInfo(uid,uname,self_intro,avatar,isValueUser,Tags) "
        "VALUES(?,?,?,?,?,?)");
    Q.bind(1, uid);
    Q.bind(2, uname);
    Q.bind(3, intro);
    Q.bind(4, avatar);
    Q.bind(5, (isValueUser == 1) ? "1" : "0");
    vector<web::json::value> vals;
      web::json::array tagsArr= core.user_getUserTag(uid)["data"].as_array();
      for(web::json::value x:tagsArr){
          vals.push_back(x["ut_name"]);
      }
    Q.bind(6, web::json::value::array(vals).serialize());
    Q.executeStep();
    web::json::value j;
    j["uid"] = web::json::value::string(uid);
    j["uname"] = web::json::value::string(uname);
    j["intro"] = web::json::value::string(intro);
    j["avatar"] = web::json::value::string(avatar);
    j["isValueUser"] = web::json::value::boolean(isValueUser != 0);
    j["Tags"] = web::json::value::array(vals);
    return j;
  }
}
string DownloadUtils::loadOrSaveGroupName(string name, string GID) {
  std::lock_guard<mutex> guard(dbLock);
  Database DB(DBPath, SQLite::OPEN_READWRITE);
  Statement Q(DB, "SELECT GroupName FROM GroupInfo WHERE gid=(?)");
  Q.bind(1, GID);
  Q.executeStep();
  if (Q.hasRow()) {
    return Q.getColumn(0).getString();
  } else {
    Statement insertQuery(
        DB, "INSERT INTO GroupInfo (gid, GroupName) VALUES (?,?)");
    insertQuery.bind(1, GID);
    insertQuery.bind(2, name);
    insertQuery.executeStep();
    return name;
  }
}
DownloadUtils::Info
DownloadUtils::canonicalizeRawServerDetail(web::json::value Inf) {
  web::json::value tagsJSON = Inf["post_tags"];
  string item_id = ensure_string(Inf["item_id"]);
  string uid = ensure_string(Inf["uid"]);
  string desc = ensure_string(Inf["plain"]);
  vector<string> tags; // Inner Param
  for (web::json::value tagD : Inf["post_tags"].as_array()) {
    string tag = tagD["tag_name"].as_string();
    tags.push_back(tag);
  }
  string title;
  if (Inf.has_field("title") && Inf["title"].as_string() != "") {
    title = Inf["title"].as_string();
  } else {
    if (Inf.has_field("post_core") && Inf.at("post_core").has_field("name") &&
        Inf["post_core"]["name"].is_string()) {
      title = Inf["post_core"]["name"].as_string();
    } else {
      if (Inf.has_field("ud_id")) {
        string val = ensure_string(Inf["ud_id"]);
        title = "日常-" + val;
      } else if (Inf.has_field("cp_id")) {
        string val = ensure_string(Inf["cp_id"]);
        title = "Cosplay-" + val;
      } else if (Inf.has_field("dp_id")) {
        string val = ensure_string(Inf["dp_id"]);
        title = "绘画" + val;
      } else if (Inf.has_field("post_id")) {
        string GID = ensure_string(Inf["group"]["gid"]);
        string GroupName =
            loadOrSaveGroupName(Inf["group"]["name"].as_string(), GID);
        string val = "";
        val = ensure_string(Inf["ud_id"]);
        title = GroupName + "-" + val;
      } else if (Inf.has_field("item_id")) {
        string val = ensure_string(Inf["item_id"]);
        if (Inf["type"].as_string() == "works") {
          title = "Cosplay-" + val;
        } else if (Inf["type"].as_string() == "preview") {
          title = "预告-" + val;
        } else if (Inf["type"].as_string() == "daily") {
          title = "日常-" + val;
        } else if (Inf["type"].as_string() == "video") {
          title = "视频-" + val;
        } else {
          title = val;
        }
      }
    }
  }
  if (title == "") {
    title = item_id;
  }
  title = loadTitle(title, item_id);
  vector<web::json::value> URLs;
  if (Inf.has_field("multi")) {
    for (web::json::value item : Inf["multi"].as_array()) {
      URLs.push_back(item);
    }
  }
  if (Inf.has_field("cover")) {
    web::json::value j;
    j["type"] = web::json::value("image");
    j["path"] = Inf["cover"];
    URLs.emplace_back(j);
  }
  if (Inf.has_field("content")) {
    regex rgx("<img src=\"(.{80,100})\" alt=");
    string tmpjson = Inf["content"].as_string();
    smatch matches;
    while (regex_search(tmpjson, matches, rgx)) {
      web::json::value j;
      string URL = matches[0];
      j["type"] = web::json::value("image");
      j["path"] = web::json::value(URL);
      URLs.emplace_back(j);
      tmpjson = matches.suffix();
    }
  }
  if (Inf.has_field("group") && Inf["group"].has_field("multi")) {
    for (web::json::value foo : Inf["group"]["multi"].as_array()) {
      URLs.push_back(foo);
    }
  }
  string vid;
  if (Inf.has_field("type") && Inf["type"].as_string() == "video" &&
      Inf.has_field("video_info") && downloadVideo == true) {
    vid = Inf["video_info"]["vid"].as_string();
  }

  saveOrLoadUser(uid, ensure_string(Inf["profile"]["uname"]), desc,
                 ensure_string(Inf["profile"]["avatar"]),
                 Inf["profile"]["value_user"].as_bool());
  web::json::array multi = web::json::value::array(URLs).as_array();
  auto tup = std::tuple<std::string /*UID*/, std::string /*item_id*/,
                        std::string /*Title*/, vector<string> /*Tags*/,
                        std::string /*ctime*/, std::string /*Description*/,
                        web::json::array /*multi*/, std::string /*videoID*/>(
      uid, item_id, title, tags, ensure_string(Inf["ctime"]), desc, multi, vid);
  boost::this_thread::interruption_point();
  BOOST_LOG_TRIVIAL(debug) << "Saving Info For: " << title << endl;
  saveInfo(tup);
  boost::this_thread::interruption_point();
  return tup;
}
string DownloadUtils::loadTitle(string title, string item_id) {
  string query = "SELECT Title FROM ItemInfo WHERE item_id=(?)";
  std::lock_guard<mutex> guard(dbLock);
  Database DB(DBPath, OPEN_READONLY);
  Statement Q(DB, query);
  Q.bind(1, item_id);
  boost::this_thread::interruption_point();
  Q.executeStep();
  if (Q.hasRow()) {
    string T = Q.getColumn("Title").getString();
    if (T != "") {
      return T;
    }
  }
  return title;
}
void DownloadUtils::saveInfo(DownloadUtils::Info Inf){
    vector<string> vals;
    string query = "INSERT INTO ItemInfo "
                   "(uid,item_id,Title,Tags,ctime,Description,Images,VideoID) "
                   "VALUES(?,?,?,?,?,?,?,?)";
    vals.push_back(std::get<0>(Inf));
    vals.push_back(std::get<1>(Inf));
    vals.push_back(std::get<2>(Inf));
    vector<string> tags=std::get<3>(Inf);
    vector<web::json::value> tmps;
    for(string foo:tags){
        tmps.push_back(web::json::value(foo));
    }
    vals.push_back(web::json::value::array(tmps).serialize());
    vals.push_back(std::get<4>(Inf));
      vals.push_back(std::get<5>(Inf));
    tmps.clear();
    for(web::json::value x:std::get<6>(Inf)){
        tmps.push_back(x);
    }
    vals.push_back(web::json::value::array(tmps).serialize());
    vals.push_back(std::get<7>(Inf));
    std::lock_guard<mutex> guard(dbLock);
    Database DB(DBPath, SQLite::OPEN_READWRITE);
    Statement Q(DB, query);
    for (decltype(vals.size()) i = 0; i < vals.size(); i++) {
      Q.bind(i + 1, vals[i]);
    }
    Q.executeStep();
} optional<DownloadUtils::Info> DownloadUtils::loadInfo(string item_id) {
  std::lock_guard<mutex> guard(dbLock);
  Database DB(DBPath, SQLite::OPEN_READONLY);
  Statement Q(
      DB, "SELECT uid,item_id,Title,Tags,ctime,Description,Images,VideoID FROM "
          "ItemInfo WHERE item_id=?");
  Q.bind(1, item_id);
  Q.executeStep();
  if (Q.hasRow()) {
    web::json::value v;
    string uid = Q.getColumn(0).getString();
    string item_id = Q.getColumn(1).getString();
    string title = Q.getColumn(2).getString();
    web::json::array tagsArr =
        web::json::value::parse(Q.getColumn(3).getString()).as_array();
    vector<string> tags;
    for (web::json::value val : tagsArr) {
      tags.push_back(val.as_string());
    }
    string ctime = Q.getColumn(4).getString();
    string desc = Q.getColumn(5).getString();
    web::json::array multi =
        web::json::value::parse(Q.getColumn(6).getString()).as_array();
    string videoID = Q.getColumn(7).getString();
    auto tup = std::tuple<std::string /*UID*/, std::string /*item_id*/,
                          std::string /*Title*/, vector<string> /*Tags*/,
                          std::string /*ctime*/, std::string /*Description*/,
                          web::json::array /*multi*/, std::string /*videoID*/>(
        uid, item_id, title, tags, ctime, desc, multi, videoID);
    return tup;
  } else {
    return {};
  }
}
void DownloadUtils::downloadEvent(string event_id) {
  //  DB.exec("CREATE TABLE IF NOT EXISTS EventInfo (event_id INTEGER,etime
  //  INTEGER,stime INTEGER,cover STRING,intro STRING,UNIQUE(event_id) ON
  //  CONFLICT IGNORE)");
  web::json::value det = loadEventInfo(event_id);
  if (det.is_null()) {
    det = core.event_detail(event_id)["data"];
    insertEventInfo(det);
  }
  core.event_listPosts(event_id, Order::Index, downloadCallback);
}
web::json::value DownloadUtils::loadEventInfo(string event_id) {
  std::lock_guard<mutex> guard(dbLock);
  Database DB(DBPath, SQLite::OPEN_READWRITE);
  Statement Q(DB, "SELECT Info FROM EventInfo WHERE event_id=?");
  Q.bind(1, event_id);
  Q.executeStep();
  if (Q.hasRow()) {
    return web::json::value::parse(Q.getColumn(0).getString());
  } else {
    return web::json::value();
  }
}
void DownloadUtils::insertEventInfo(web::json::value Inf) {
  std::lock_guard<mutex> guard(dbLock);
  Database DB(DBPath, SQLite::OPEN_READWRITE);
  Statement insertQuery(
      DB, "INSERT INTO EventInfo (event_id,etime,stime,cover,intro,Info) "
          "VALUES (?,?,?,?,?,?)");
  insertQuery.bind(1, Inf["event_id"].as_integer());
  insertQuery.bind(2, Inf["etime"].as_integer());
  insertQuery.bind(3, Inf["stime"].as_integer());
  insertQuery.bind(4, Inf["cover"].as_string());
  insertQuery.bind(5, Inf["intro"].as_string());
  insertQuery.bind(6, Inf.serialize());
  insertQuery.executeStep();
}
void DownloadUtils::downloadFromInfo(DownloadUtils::Info Inf, bool runFilter) {
  /*
  Title usually contains UTF8 characters which causes trouble on certain
  platforms. (I'm looking at you Windowshit) Migrate to item_id based ones
  */
  if(runFilter && filter->shouldBlockItem(Inf)){
    return;
  }
  string uid = std::get<0>(Inf);
  string item_id = std::get<1>(Inf);
  string title = std::get<2>(Inf);
  std::vector<std::string> tags = std::get<3>(Inf);
  string ctime = std::get<4>(Inf);
  string desc = std::get<5>(Inf);
  vector<web::json::value> multi;
  string videoID = std::get<7>(Inf);
  for (web::json::value x : std::get<6>(Inf)) {
    multi.push_back(x);
  }
  fs::path savePath = getItemPath(uid, item_id);

  if (multi.size() > 0) {
    boost::system::error_code ec;
    fs::create_directories(savePath, ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "FileSystem Error: " << ec.message() << "@"
                               << __FILE__ << ":" << __LINE__ << endl;
    }
  } else {
    BOOST_LOG_TRIVIAL(error) << item_id << " has no item to download" << endl;
    return;
  }
  vector<web::json::value>
      a2Methods; // Used for aria2 RPC's ``system.multicall``

  // videoInfo
  if (videoID != "" && downloadVideo == true) {
    boost::this_thread::interruption_point();
    web::json::value F = core.videoInfo(videoID);
    boost::this_thread::interruption_point();
    web::json::value videoList = F["data"]["video_list"];
    // Find the most HD one
    int bitrate = 0;
    string vid = "video_1";
    if (videoList.is_null()) {
      BOOST_LOG_TRIVIAL(error)
          << "Can't query DownloadURL for videoID:" << vid << endl;
    } else {
      for (auto it = videoList.as_object().cbegin();
           it != videoList.as_object().cend(); ++it) {
        string K = it->first;
        web::json::value V = it->second;
        if (V["bitrate"].as_integer() > bitrate) {
          vid = K;
        }
      }
      if (videoList.size() > 0) {
        // Videos needs to be manually reviewed before playable
        string URL = "";
        Base64::Decode(videoList[vid]["main_url"].as_string(), &URL);
        string FileName = videoID + ".mp4";
        web::json::value j;
        j["path"] = web::json::value(URL);
        j["FileName"] = web::json::value(FileName);

        multi.push_back(j);
      } else {
        BOOST_LOG_TRIVIAL(info)
            << item_id << " hasn't been reviewed yet and thus not downloadable"
            << endl;
      }
    }
  }
  for (web::json::value item : multi) {
    boost::this_thread::interruption_point();
    string URL = item["path"].as_string();
    if (URL.find("http") != 0) {
      // Some old API bug that results in rubbish URL in response
      // Because ByteDance sucks dick
      continue;
    }
    if (URL.length() == 0) {
      continue;
    }
    string origURL = URL;
    string tmp = URL.substr(URL.find_last_of("/"), string::npos);
    if (tmp.find(".") == string::npos) {
      origURL = URL.substr(0, URL.find_last_of("/"));
    }
    string FileName = origURL.substr(origURL.find_last_of("/") + 1);

    if (item.has_field("FileName")) { // Support Video Downloading without
                                      // Introducing Extra
      // Code
      FileName = item["FileName"].as_string();
      origURL = item["path"].as_string();
    }
    fs::path newFilePath = savePath / fs::path(FileName);
    if (!newFilePath.has_extension()) {
      newFilePath.replace_extension(".jpg");
    }
    boost::system::error_code ec2;
    auto newa2confPath = fs::path(newFilePath.string() + ".aria2");

    bool shouldDL =
        (!fs::exists(newFilePath, ec2) || fs::exists(newa2confPath, ec2));
    if (shouldDL) {
      if (RPCServer == "") {
        if (stop) {
          return;
        }
        fs::remove(newa2confPath, ec2);
        fs::remove(newFilePath, ec2);
        boost::asio::post(*downloadThread, [=]() {
          if (stop) {
            return;
          }
          try {
            boost::this_thread::interruption_point();
            auto R = core.GET(origURL);
            boost::this_thread::interruption_point();
            ofstream ofs(newFilePath.string(), ios::binary);
            auto vec = R.extract_vector().get();
            ofs.write(reinterpret_cast<const char *>(vec.data()), vec.size());
            ofs.close();
            boost::this_thread::interruption_point();
          } catch (const std::exception &exp) {
            BOOST_LOG_TRIVIAL(error)
                << "Download from " << origURL
                << " failed with exception:" << exp.what() << endl;
          }
        });
      } else {
        boost::this_thread::interruption_point();
        web::json::value rpcparams;
        vector<web::json::value> params; // Inner Param
        vector<web::json::value> URLs;
        URLs.push_back(web::json::value(origURL));
        if (secret != "") {
          params.push_back(web::json::value("token:" + secret));
        }
        params.push_back(web::json::value::array(URLs));
        web::json::value options;
        options["dir"] = web::json::value(savePath.string());
        options["out"] = web::json::value(newFilePath.filename().string());
        options["auto-file-renaming"] = web::json::value("false");
        options["allow-overwrite"] = web::json::value("false");
        // options["user-agent"] = "bcy 4.3.2 rv:4.3.2.6146 (iPad; iPhone
        // OS 9.3.3; en_US) Cronet";
        string gid = md5(origURL).substr(0, 16);
        options["gid"] = web::json::value(gid);
        params.push_back(options);
        if (item.has_field("FileName")) {
          /*
           Video URLs have a (very short!) valid time window
           so we insert those to the start of download queue
          */
          params.push_back(web::json::value(1));
        }
        rpcparams["params"] = web::json::value::array(params);
        rpcparams["methodName"] = web::json::value("aria2.addUri");
        a2Methods.push_back(rpcparams);
        boost::this_thread::interruption_point();
      }
    }
  }
  if (a2Methods.size() > 0) {
    try {

      web::json::value arg;
      arg["jsonrpc"] = web::json::value("2.0");
      arg["id"] = web::json::value();
      arg["method"] = web::json::value("system.multicall");
      vector<web::json::value> tmp;
      tmp.push_back(web::json::value::array(a2Methods));
      arg["params"] = web::json::value::array(tmp);
      web::http::client::http_client client(RPCServer);
      web::json::value rep =
          client.request(web::http::methods::POST, U("/"), arg)
              .get()
              .extract_json(true)
              .get();

      if (rep.has_field("result")) {
        BOOST_LOG_TRIVIAL(debug)
            << item_id
            << " Registered in Aria2 with GID:" << rep["result"].serialize()
            << " Query:" << arg.serialize() << endl;
      } else {
        BOOST_LOG_TRIVIAL(error)
            << item_id << " Failed to Register with Aria2. Response:"
            << rep["error"]["message"].as_string()
            << " Query:" << arg.serialize() << endl;
      }
    } catch (const std::exception &exp) {
      BOOST_LOG_TRIVIAL(error)
          << "Posting to Aria2 Error:" << exp.what() << endl;
    }
  }
}
void DownloadUtils::verifyUID(string UID, bool reverse) {
  verify("WHERE uid=?", {UID}, reverse);
}
void DownloadUtils::verifyTag(string Tag, bool reverse) {
  verify("WHERE Tags LIKE ?", {"%" + Tag + "%"}, reverse);
}
void DownloadUtils::unlikeCached() {
  vector<web::json::value> Liked = core.space_getUserLikeTimeLine(core.UID);
  BOOST_LOG_TRIVIAL(info) << "Found " << Liked.size() << " Liked Works" << endl;
  thread_pool t(16);
  for (web::json::value j : Liked) {
    string item_id = ensure_string(j["item_detail"]["item_id"]);
    boost::asio::post(t, [=] {
      if (loadInfo(item_id).has_value()) {
        if (core.item_cancelPostLike(item_id)) {
          BOOST_LOG_TRIVIAL(info)
              << "Unlike item_id: " << item_id << " Success" << endl;
        } else {
          BOOST_LOG_TRIVIAL(info)
              << "Unlike item_id: " << item_id << " Failed" << endl;
        }
      } else {
        BOOST_LOG_TRIVIAL(info)
            << "item_id: " << item_id << " Not Found In Local Cache" << endl;
      }
    });
  }
  BOOST_LOG_TRIVIAL(info) << "Joining Unlike Threads\n";
  t.join();
}
    void DownloadUtils::verify(std::string condition, std::vector<std::string> args,
                               bool reverse ){
        
        BOOST_LOG_TRIVIAL(info) << "Verifying..." << endl;
        vector<string> IDs;
        BOOST_LOG_TRIVIAL(info) << "Collecting Cached Infos" << endl;
        {
            std::lock_guard<mutex> guard(dbLock);
            Database DB(DBPath, SQLite::OPEN_READONLY);
            Statement Q(DB, "SELECT item_id FROM ItemInfo " + condition);
            for (decltype(args.size()) i = 1; i <= args.size(); i++) {
                Q.bind(i, args[i - 1]);
            }
            while (Q.executeStep()) {
                IDs.push_back(Q.getColumn(0).getString());
            }
        }
        BOOST_LOG_TRIVIAL(info) << "Found "<<IDs.size()<<" items" << endl;
        unsigned long long idx=0;
        if(reverse){
            vector<string>::iterator i = IDs.end();
            while (i != IDs.begin())
            {
                --i;
                string item_id=*i;
                DownloadUtils::Info inf=*(loadInfo(item_id));
                downloadFromInfo(inf);
                idx++;
                if(idx%500==0){
                    BOOST_LOG_TRIVIAL(info) << "Verified "<<idx<<" items" << endl;
                }
                
            }
        }
        else{
            for(string item_id:IDs){
                DownloadUtils::Info inf=*(loadInfo(item_id));
                downloadFromInfo(inf);
                idx++;
                if(idx%500==0){
                    BOOST_LOG_TRIVIAL(info) << "Verified "<<idx<<" items" << endl;
                }
            }
        }
        
}
void DownloadUtils::cleanUID(string UID) {
  BOOST_LOG_TRIVIAL(debug) << "Cleaning up UID:" << UID << endl;
  boost::system::error_code ec;
  fs::path UserPath = getUserPath(UID);
  bool isDirec = is_directory(UserPath, ec);
  if (isDirec) {
    fs::remove_all(UserPath, ec);
    BOOST_LOG_TRIVIAL(debug) << "Removed " << UserPath.string() << endl;
  }

  std::lock_guard<mutex> guard(dbLock);
  Database DB(DBPath, SQLite::OPEN_READWRITE);
  Statement Q(DB, "DELETE FROM ItemInfo WHERE UID=" + UID);
  Q.executeStep();
}
void DownloadUtils::cleanItem(string item_id) {
  BOOST_LOG_TRIVIAL(debug) << "Cleaning up Item:" << item_id << endl;
  std::lock_guard<mutex> guard(dbLock);
  Database DB(DBPath, SQLite::OPEN_READWRITE);
  Statement Q(DB, "DELETE FROM ItemInfo WHERE item_id=" + item_id);
  Q.executeStep();
}
fs::path DownloadUtils::getUserPath(string UID) {
  while (UID.length() < 3) {
    UID = "0" + UID;
  }
  string L1Path = string(1, UID[0]);
  string L2Path = string(1, UID[1]);
  fs::path UserPath =
      fs::path(saveRoot) / fs::path(L1Path) / fs::path(L2Path) / fs::path(UID);
  return UserPath;
}
fs::path DownloadUtils::getItemPath(string UID, string item_id) {
  return getUserPath(UID) / fs::path(item_id);
}
void DownloadUtils::cleanTag(string Tag) {
  BOOST_LOG_TRIVIAL(info) << "Cleaning up Tag:" << Tag << endl;
  std::lock_guard<mutex> guard(dbLock);
  Database DB(DBPath, SQLite::OPEN_READWRITE);
  Statement Q(DB,
              "SELECT UID,Title,Info,item_id FROM ItemInfo WHERE Tags Like ?");
  Q.bind(1, "%%\"" + Tag + "\"%%");
  while (Q.executeStep()) {
    string UID = Q.getColumn(0).getString();
    string Title = Q.getColumn(1).getString();
    string Info = Q.getColumn(2).getString();
    string item_id = Q.getColumn(3).getString();
    if (item_id == "" || item_id == "0") {
      continue;
    }
    boost::system::error_code ec;
    fs::path UserPath = getUserPath(UID);
    bool isDirec = is_directory(UserPath, ec);
    if (isDirec) {
      fs::remove_all(UserPath, ec);
      if (ec) {
        BOOST_LOG_TRIVIAL(error) << "FileSystem Error: " << ec.message() << "@"
                                 << __FILE__ << ":" << __LINE__ << endl;
      } else {
        BOOST_LOG_TRIVIAL(debug) << "Removed " << UserPath.string() << endl;
      }
    }
  }
}
void DownloadUtils::cleanup() {
  BOOST_LOG_TRIVIAL(info) << "Cleaning up..." << endl;
  thread_pool t(16);
  for (decltype(filter->UIDList.size()) i = 0; i < filter->UIDList.size();
       i++) {
    boost::asio::post(t, [=]() {
      string UID = filter->UIDList[i];
      BOOST_LOG_TRIVIAL(debug) << "Removing UID: " << UID << endl;
      cleanUID(UID);
    });
  }
  // vector<string> Infos;
  for (decltype(filter->TagList.size()) i = 0; i < filter->TagList.size();
       i++) {
    boost::asio::post(t, [=]() {
      string Tag = filter->TagList[i];
      BOOST_LOG_TRIVIAL(debug) << "Removing Tag: " << Tag << endl;
      cleanTag(Tag);
    });
  }
  t.join();
}
string DownloadUtils::md5(string &str) {
  string digest;
  Weak::MD5 md5;
  StringSource(str, true,
               new HashFilter(md5, new HexEncoder(new StringSink(digest))));
  return digest;
}
void DownloadUtils::downloadLiked() {
  if (core.UID != "") {
    downloadUserLiked(core.UID);
  } else {
    BOOST_LOG_TRIVIAL(error)
        << "Not Logged In. Can't Download Liked Work" << endl;
  }
}
void DownloadUtils::downloadSearchKeyword(string KW) {
  BOOST_LOG_TRIVIAL(info) << "Iterating Searched Works For Keyword:" << KW
                          << endl;
  auto l = core.search(KW, SearchType::Content, downloadCallback);

  BOOST_LOG_TRIVIAL(info) << "Found " << l.size()
                          << " Searched Works For Keyword:" << KW << endl;
}
void DownloadUtils::downloadUser(string uid) {
  BOOST_LOG_TRIVIAL(info) << "Iterating Original Works For UserID:" << uid
                          << endl;
  auto l = core.timeline_getUserPostTimeLine(uid, downloadCallback);
  BOOST_LOG_TRIVIAL(info) << "Found " << l.size()
                          << " Original Works For UserID:" << uid << endl;
}
void DownloadUtils::downloadTag(string TagName) {
  if (typeFilters.size() == 0) {
    BOOST_LOG_TRIVIAL(info) << "Iterating Works For Tag:" << TagName << endl;
    auto coser = core.circle_itemrecenttags(TagName, "all", downloadCallback);
    BOOST_LOG_TRIVIAL(info)
        << "Found " << coser.size() << " Works For Tag:" << TagName << endl;
  } else {
    web::json::value rep = core.tag_status(TagName);
    if (rep.is_null()) {
      BOOST_LOG_TRIVIAL(error)
          << "Status for Tag:" << TagName << " is null" << endl;
      return;
    }
    int foo = rep["data"]["tag_id"].as_integer();
    string circle_id = to_string(foo);
    web::json::value FilterList =
        core.circle_filterlist(circle_id, CircleType::Tag, TagName);
    if (FilterList.is_null()) {
      BOOST_LOG_TRIVIAL(error)
          << "FilterList For Tag:" << TagName << " is null";
      return;
    }
    FilterList = FilterList["data"];
    for (web::json::value j : FilterList.as_array()) {
      string name = j["name"].as_string();
      int id = 0;
      string idstr = j["id"].as_string();
      if (idstr != "") {
        id = std::stoi(idstr);
      }
      if (find(typeFilters.begin(), typeFilters.end(), name) !=
          typeFilters.end()) {
        if (id >= 1 && id <= 3) { // First class
          BOOST_LOG_TRIVIAL(info) << "Iterating Works For Tag:" << TagName
                                  << " and Filter:" << name << endl;
          auto foo = core.search_item_bytag({name}, static_cast<PType>(id),
                                            downloadCallback);
          BOOST_LOG_TRIVIAL(info)
              << "Found " << foo.size() << " Works For Tag:" << TagName
              << " and Filter:" << name << endl;
        } else {
          BOOST_LOG_TRIVIAL(info) << "Iterating Works For Tag:" << TagName
                                  << " and Filter:" << name << endl;
          auto foo = core.search_item_bytag({TagName, name}, PType::Undef,
                                            downloadCallback);
          BOOST_LOG_TRIVIAL(info)
              << "Found " << foo.size() << " Works For Tag:" << TagName
              << " and Filter:" << name << endl;
        }
      }
    }
  }
}
void DownloadUtils::downloadGroupID(string gid) {
  BOOST_LOG_TRIVIAL(info) << "Iterating Works For GroupID:" << gid << endl;
  auto l = core.group_listPosts(gid, downloadCallback);
  BOOST_LOG_TRIVIAL(info) << "Found " << l.size()
                          << " Works For GroupID:" << gid << endl;
}
void DownloadUtils::downloadUserLiked(string uid) {
  if (uid == core.UID) {
    BOOST_LOG_TRIVIAL(info)
        << "Iterating Liked Works For UserID:" << uid << endl;
    auto l = core.space_getUserLikeTimeLine(uid, [&](web::json::value j) {
      this->downloadFromAbstractInfo(j, false);
      return true;
    });
    BOOST_LOG_TRIVIAL(info)
        << "Found " << l.size() << " Liked Works For UserID:" << uid << endl;
    return;
  }
  BOOST_LOG_TRIVIAL(info) << "Iterating Liked Works For UserID:" << uid << endl;
  auto l = core.space_getUserLikeTimeLine(uid, downloadCallback);
  BOOST_LOG_TRIVIAL(info) << "Found " << l.size()
                          << " Liked Works For UserID:" << uid << endl;
}
void DownloadUtils::downloadItemID(string item_id) {
  if (stop) {
    return;
  }
  boost::asio::post(*queryThread, [=]() {
    if (stop) {
      return;
    }
    boost::this_thread::interruption_point();
      optional<DownloadUtils::Info> detail;
    if (useCachedInfo) {
      detail=loadInfo(item_id);
    }
    if (detail.has_value()==false) {
      boost::this_thread::interruption_point();
      detail.emplace(canonicalizeRawServerDetail(core.item_detail(item_id)["data"]));
      boost::this_thread::interruption_point();
      if (detail.has_value()==false) {
        BOOST_LOG_TRIVIAL(error) << "Querying detail for item_id:" << item_id
                                 << " results in null" << endl;
      }
      try {
        downloadFromInfo(*detail,enableFilter);
      } catch (boost::thread_interrupted) {
        BOOST_LOG_TRIVIAL(debug)
            << "Cancelling Thread:" << boost::this_thread::get_id() << endl;
        return;
      } catch (const exception &exc) {
        BOOST_LOG_TRIVIAL(error)
          << "Downloading from Info:" << std::get<1>(*detail)
            << " Raised Exception:" << exc.what() << endl;
      }
    } else {
      try {
        downloadFromInfo(*detail,enableFilter);
      } catch (boost::thread_interrupted) {
        BOOST_LOG_TRIVIAL(debug)
            << "Cancelling Thread:" << boost::this_thread::get_id() << endl;
        return;
      } catch (const exception &exc) {
        BOOST_LOG_TRIVIAL(error)
            << "Downloading from Info:" << std::get<1>(*detail)
            << " Raised Exception:" << exc.what() << endl;
      }
    }
  });
}
void DownloadUtils::downloadHotTags(string TagName, unsigned int cnt) {
  unsigned int c = 0;
  core.circle_itemhottags(TagName, [&](web::json::value j) {
    this->downloadFromAbstractInfo(j, enableFilter);
    c++;
    if (c < cnt) {
      return true;
    }
    return false;
  });
}
void DownloadUtils::downloadWorkID(string item) {
  if (typeFilters.size() == 0) {
    BOOST_LOG_TRIVIAL(info) << "Iterating Works For WorkID:" << item << endl;
    auto l = core.circle_itemRecentWorks(item, downloadCallback);
    BOOST_LOG_TRIVIAL(info)
        << "Found " << l.size() << " Works For WorkID:" << item << endl;
  } else {
    web::json::value rep = core.core_status(item);
    if (rep.is_null()) {
      BOOST_LOG_TRIVIAL(error)
          << "Status for WorkID:" << item << " is null" << endl;
      return;
    }
    string WorkName = rep["data"]["real_name"].as_string();
    web::json::value FilterList =
        core.circle_filterlist(item, CircleType::Work, WorkName);
    if (FilterList.is_null()) {
      BOOST_LOG_TRIVIAL(error)
          << "FilterList For WorkID:" << item << " is null";
    } else {
      FilterList = FilterList["data"];
    }
    for (web::json::value j : FilterList.as_array()) {
      string name = j["name"].as_string();
      int id = 0;
      string idstr = j["id"].as_string();
      if (idstr != "") {
        id = std::stoi(idstr);
      }
      if (find(typeFilters.begin(), typeFilters.end(), name) !=
          typeFilters.end()) {
        if (id >= 1 && id <= 3) { // First class
          BOOST_LOG_TRIVIAL(info) << "Iterating Works For WorkID:" << item
                                  << " and Filter:" << name << endl;
          auto foo = core.search_item_bytag({name}, static_cast<PType>(id),
                                            downloadCallback);
          BOOST_LOG_TRIVIAL(info)
              << "Found " << foo.size() << " Works For WorkID:" << item
              << " and Filter:" << name << endl;
        } else {
          BOOST_LOG_TRIVIAL(info) << "Iterating Works For WorkID:" << item
                                  << " and Filter:" << name << endl;
          auto foo = core.search_item_bytag({WorkName, name}, PType::Undef,
                                            downloadCallback);
          BOOST_LOG_TRIVIAL(info)
              << "Found " << foo.size() << " Works For WorkID:" << item
              << " and Filter:" << name << endl;
        }
      }
    }
  }
}
void DownloadUtils::addTypeFilter(string filter) { typeFilters.insert(filter); }
void DownloadUtils::downloadTimeline() {
  BOOST_LOG_TRIVIAL(info) << "Downloading Friend Feed" << endl;
  core.timeline_friendfeed(downloadCallback);
}
void DownloadUtils::join() {
  BOOST_LOG_TRIVIAL(info) << "Joining Query Threads" << endl;
  queryThread->join();
  BOOST_LOG_TRIVIAL(info) << "Joining Download Threads" << endl;
  downloadThread->join();
}
void DownloadUtils::downloadHotWorks(std::string id, unsigned int cnt) {
  unsigned int c = 0;
  core.circle_itemhotworks(id, [&](web::json::value j) {
    this->downloadFromAbstractInfo(j, enableFilter);
    c++;
    if (c < cnt) {
      return true;
    }
    return false;
  });
}
DownloadUtils::~DownloadUtils() {
  stop = true;
  /*
   As long as database is commited properly, everything else could be
   just killed. Boost's thread_pool implementation doesn't provide access to
   underlying boost::thread_group object
   */
  BOOST_LOG_TRIVIAL(info) << "Canceling Query Threads..." << endl;
  queryThread->stop();
  delete queryThread;
  queryThread = nullptr;
  BOOST_LOG_TRIVIAL(info) << "Canceling Download Threads..." << endl;
  downloadThread->stop();
  delete downloadThread;
  downloadThread = nullptr;
  {
    BOOST_LOG_TRIVIAL(info) << "Waiting Database Lock" << endl;
    std::lock_guard<mutex> guard(dbLock);
    BOOST_LOG_TRIVIAL(info) << "Saving Filters..." << endl;
    delete filter;
    filter = nullptr;
  }
}

} // namespace BCY
