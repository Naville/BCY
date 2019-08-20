#include <BCY/Base64.h>
#include <BCY/Core.hpp>
#include <BCY/DownloadFilter.hpp>
#include <BCY/DownloadUtils.hpp>
#include <BCY/Utils.hpp>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/program_options.hpp>
#include <chaiscript/chaiscript.hpp>
#include <chaiscript/utility/json.hpp>
#include <chrono>
#include <cpprest/json.h>
#include <fstream>
#include <random>
using namespace BCY;
using namespace std::placeholders;
using namespace std;
using namespace boost;
namespace bfs = boost::filesystem;
namespace po = boost::program_options;
namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

static DownloadUtils *DU = nullptr;
static po::variables_map vm;
static po::positional_options_description pos;
static web::json::value config;
static string Prefix =
    "BCYDownloader"; // After Login we replace this with UserName
typedef std::function<void(vector<string>)> commHandle;
static map<string, commHandle> handlers;
static map<string, string> handlerMsgs;
void JSONMode();
void cleanup(int sig) {
  if (DU != nullptr) {
    delete DU;
    DU = nullptr;
  }
  kill(getpid(), SIGKILL);
}
void cleanup2() {
  if (DU != nullptr) {
    delete DU;
    DU = nullptr;
  }
}
void cleanupHandle() { DU->cleanup(); }
static void quit() {
  if (DU != nullptr) {
    delete DU;
    DU = nullptr;
  }
  exit(0);
}
static void liked(string UID) {
  if (UID != "") {
    DU->downloadUserLiked(UID);
  } else {
    DU->downloadLiked();
  }
}
static void unlike() { DU->unlikeCached(); }
static void downloadVideo(bool flag) { DU->downloadVideo = flag; }
static void process() { JSONMode(); }
static void aria2(string addr, string secret) {
  DU->RPCServer = addr;
  DU->secret = secret;
}
static void init(string path, int queryCnt, int downloadCnt, string DBPath) {
  if (DU == nullptr) {
    try {
      DU = new DownloadUtils(path, queryCnt, downloadCnt, DBPath);
    } catch (const SQLite::Exception &ex) {
      cout << "Database Error:" << ex.getErrorStr() << endl
           << "Error Code:" << ex.getErrorCode() << endl
           << "Extended Error Code:" << ex.getExtendedErrorCode() << endl;
      abort();
    }
    signal(SIGINT, cleanup);
  } else {
    cout << "Already Initialized" << endl;
  }
}
static void loginWithSKey(string uid, string SKey) {
  DU->core.loginWithUIDAndSessionKey(uid, SKey);
}
static void hotTags(string tagName, unsigned int cnt) {
  DU->downloadHotTags(tagName, cnt);
}
static void hotWorks(string id, unsigned int cnt) {
  DU->downloadHotWorks(id, cnt);
}
static void addtype(string typ) { DU->addTypeFilter(typ); }
static void user(string UID) { DU->downloadUser(UID); }
static void tag(string tag) { DU->downloadTag(tag); }
static void work(string workid) { DU->downloadWorkID(workid); }
static void item(string item_id) { DU->downloadItemID(item_id); }
static void proxy(string proxy) { DU->core.proxy = proxy; }
static void group(string gid) { DU->downloadGroupID(gid); }
static void cleanup() { DU->cleanup(); }
static void joinHandle() { DU->join(); }
static void toggleFilter(bool stat) { DU->enableFilter = stat; }
static void login(string email, string password) {
  if (DU->core.UID == "") {
#ifndef DEBUG
    try {
#endif
      web::json::value Res =
          DU->core.loginWithEmailAndPassword(email, password);
      if (!Res.is_null()) {
        Prefix = Res["data"]["uname"].as_string();
      } else {
        cout << "Login Failed" << endl;
      }
#ifndef DEBUG
    } catch (std::exception &exc) {
      cout << "Exception:" << exc.what() << endl;
    }
#endif
  } else {
    cout << "Already logged in as UID:" << DU->core.UID << endl;
  }
}
static void verifyUID(string UID, bool reverse) { DU->verifyUID(UID, reverse); }
static void verify(bool reverse) { DU->verify("", {}, reverse); }
static void forcequit() { kill(getpid(), 9); }
static void searchKW(string kw) { DU->downloadSearchKeyword(kw); }
static void dEvent(string event_id) { DU->downloadEvent(event_id); }
static void verifyTag(string TagName, bool reverse) {
  DU->verifyTag(TagName, reverse);
}
static void uploadWork(vector<chaiscript::Boxed_Value> paths,
                       vector<chaiscript::Boxed_Value> tags, string content) {
  vector<struct UploadImageInfo> Infos;
  vector<string> Tags;
  for (chaiscript::Boxed_Value bv : tags) {
    Tags.push_back(chaiscript::boxed_cast<string>(bv));
  }
  for (chaiscript::Boxed_Value bv : paths) {
    string path = chaiscript::boxed_cast<string>(bv);
    std::vector<char> vec;
    ifstream file(path);
    file.seekg(0, std::ios_base::end);
    std::streampos fileSize = file.tellg();
    vec.resize(fileSize);
    file.seekg(0, std::ios_base::beg);
    file.read(&vec[0], fileSize);
    auto j = DU->core.qiniu_upload(DU->core.item_postUploadToken(), vec);
    struct UploadImageInfo UII;
    UII.URL = j["key"].as_string();
    UII.Height = j["h"].as_double();
    UII.Width = j["w"].as_double();
    UII.Ratio = 0.0;
    Infos.push_back(UII);
  }
  web::json::value req = DU->core.prepareNoteUploadArg(Tags, Infos, content);
  DU->core.item_doNewPost(NewPostType::NotePost, req);
}
static void block(string OPType, string arg) {
  boost::to_upper(OPType);
  if (OPType == "UID") {
    DU->filter->UIDList.push_back(arg);
    DU->cleanUID(arg);
  } else if (OPType == "TAG") {
    DU->filter->TagList.push_back(arg);
  } else if (OPType == "USERNAME") {
    DU->filter->UserNameList.push_back(arg);
  } else if (OPType == "ITEM") {
    DU->filter->ItemList.push_back(arg);
    DU->cleanItem(arg);
  } else {
    cout << "Unrecognized OPType" << endl;
  }
}
void tags() {
  mt19937 mt_rand(time(0));
  vector<string> Tags;
  for (web::json::value j : config["Tags"].as_array()) {
    Tags.push_back(j.as_string());
  }
  shuffle(Tags.begin(), Tags.end(), mt_rand);
  for (string item : Tags) {
    try {
      DU->downloadTag(item);
    } catch (std::exception &exp) {
      cout << "Exception: " << exp.what()
           << " During Downloading of Tag:" << item << endl;
    }
  }
}
void searches() {
  mt19937 mt_rand(time(0));
  vector<string> Searches;
  for (web::json::value j : config["Searches"].as_array()) {
    Searches.push_back(j.as_string());
  }
  shuffle(Searches.begin(), Searches.end(), mt_rand);
  for (string item : Searches) {
    try {
      DU->downloadSearchKeyword(item);
    } catch (std::exception &exp) {
      cout << "Exception: " << exp.what()
           << " During Searching Keyword:" << item << endl;
    }
  }
}
void events() {
  mt19937 mt_rand(time(0));
  vector<string> Events;
  for (web::json::value j : config["Events"].as_array()) {
    Events.push_back(j.as_string());
  }
  shuffle(Events.begin(), Events.end(), mt_rand);
  for (string item : Events) {
    try {
      DU->downloadEvent(item);
    } catch (std::exception &exp) {
      cout << "Exception: " << exp.what()
           << " During Downloading Event:" << item << endl;
    }
  }
}
void works() {
  mt19937 mt_rand(time(0));
  vector<string> Works;
  for (web::json::value j : config["Works"].as_array()) {
    Works.push_back(j.as_string());
  }
  shuffle(Works.begin(), Works.end(), mt_rand);
  for (string item : Works) {
    try {
      DU->downloadWorkID(item);
    } catch (std::exception &exp) {
      cout << "Exception: " << exp.what()
           << " During Downloading WorkID:" << item << endl;
    }
  }
}
void groups() {
  vector<string> Groups;
  mt19937 mt_rand(time(0));
  for (web::json::value j : config["Groups"].as_array()) {
    Groups.push_back(j.as_string());
  }
  shuffle(Groups.begin(), Groups.end(), mt_rand);
  for (string item : Groups) {
    try {
      DU->downloadGroupID(item);
    } catch (std::exception &exp) {
      cout << "Exception: " << exp.what()
           << " During Downloading GroupID:" << item << endl;
    }
  }
}
void follows() {
  mt19937 mt_rand(time(0));
  vector<string> Follows;
  for (web::json::value j : config["Follows"].as_array()) {
    Follows.push_back(j.as_string());
  }
  shuffle(Follows.begin(), Follows.end(), mt_rand);
  for (string item : Follows) {
    try {
      DU->downloadUserLiked(item);
    } catch (std::exception &exp) {
      cout << "Exception: " << exp.what()
           << " During Downloading User Liked:" << item << endl;
    }
  }
}
void users() {
  mt19937 mt_rand(time(0));
  vector<string> Users;
  for (web::json::value j : config["Users"].as_array()) {
    Users.push_back(j.as_string());
  }
  shuffle(Users.begin(), Users.end(), mt_rand);
  for (string item : Users) {
    try {
      DU->downloadUser(item);
    } catch (std::exception &exp) {
      cout << "Exception: " << exp.what() << " During Downloading User:" << item
           << endl;
    }
  }
}
void JSONMode() {
  if (DU == nullptr) {
    cout << "You havn't initialize the downloader yet" << endl;
    return;
  }
  liked("");
  tags();
  searches();
  works();
  events();
  groups();
  follows();
  users();
  DU->downloadTimeline();
  if (config.has_field("Verify")) {
    bool ver = config["Verify"].as_bool();
    if (ver) {
      DU->verify();
    }
  }
  DU->join();
  delete DU;
  exit(0);
}
void Interactive() {
  cout << "Entering Interactive Mode..." << endl;
  chaiscript::ChaiScript engine;
  engine.add(chaiscript::fun(&block), "block");
  engine.add(chaiscript::fun(&verifyTag), "verifyTag");
  engine.add(chaiscript::fun(&init), "init");
  engine.add(chaiscript::fun(&work), "work");
  engine.add(chaiscript::fun(&item), "item");
  engine.add(chaiscript::fun(&forcequit), "forcequit");
  engine.add(chaiscript::fun(&verify), "verify");
  engine.add(chaiscript::fun(&verifyUID), "verifyUID");
  engine.add(chaiscript::fun(&login), "login");
  engine.add(chaiscript::fun(&group), "group");
  engine.add(chaiscript::fun(&aria2), "aria2");
  engine.add(chaiscript::fun(&unlike), "unlike");
  engine.add(chaiscript::fun(&liked), "liked");
  engine.add(chaiscript::fun(&addtype), "addtype");
  engine.add(chaiscript::fun(&user), "user");
  engine.add(chaiscript::fun(&proxy), "proxy");
  engine.add(chaiscript::fun(&quit), "quit");
  engine.add(chaiscript::fun(&tag), "tag");
  engine.add(chaiscript::fun(&searchKW), "search");
  engine.add(chaiscript::fun(&joinHandle), "join");
  engine.add(chaiscript::fun(&cleanupHandle), "cleanup");
  engine.add(chaiscript::fun(&downloadVideo), "downloadVideo");
  engine.add(chaiscript::fun(&dEvent), "event");
  engine.add(chaiscript::fun(&events), "events");
  engine.add(chaiscript::fun(&tags), "tags");
  engine.add(chaiscript::fun(&searches), "searches");
  engine.add(chaiscript::fun(&works), "works");
  engine.add(chaiscript::fun(&groups), "groups");
  engine.add(chaiscript::fun(&follows), "follows");
  engine.add(chaiscript::fun(&users), "users");
  engine.add(chaiscript::fun(&hotTags), "hotTags");
  engine.add(chaiscript::fun(&hotWorks), "hotWorks");
  engine.add(chaiscript::fun(&loginWithSKey), "loginWithSKey");
  engine.add(chaiscript::fun(&uploadWork), "uploadWork");
  engine.add(chaiscript::fun(&toggleFilter), "toggleFilter");
  string command;
  while (1) {
    cout << Prefix << ":$";
    getline(cin, command);
    if (!cin.good()) {
      break;
    }
#ifndef DEBUG
    try {
#endif
      engine.eval(command);
#ifndef DEBUG
    } catch (const std::exception &exc) {
      cout << "Exception:" << exc.what() << endl;
    }
#endif
  }
}
std::istream &operator>>(std::istream &in,
                         logging::trivial::severity_level &sl) {
  std::string token;
  in >> token;
  transform(token.begin(), token.end(), token.begin(), ::tolower);
  if (token == "info") {
    sl = boost::log::trivial::info;
  } else if (token == "trace") {
    sl = boost::log::trivial::trace;
  } else if (token == "debug") {
    sl = boost::log::trivial::debug;
  } else if (token == "warning") {
    sl = boost::log::trivial::warning;
  } else if (token == "error") {
    sl = boost::log::trivial::error;
  } else if (token == "fatal") {
    sl = boost::log::trivial::fatal;
  } else {
    in.setstate(std::ios_base::failbit);
  }
  return in;
}
static bool containsTag(web::json::value inf, string key) {
  if (inf.has_field(key)) {
    return true;
  }
  return false;
}
int main(int argc, char **argv) {
  logging::add_common_attributes();
  po::options_description desc("BCYDownloader Options");
  desc.add_options()("help", "Print Usage")(
      "config",
      po::value<string>()->default_value(BCY::expand_user("~/BCY.json")),
      "Initialize Downloader using JSON at provided path")(
      "i", "Interactive Console")(
      "log-level",
      po::value<logging::trivial::severity_level>()->default_value(
          logging::trivial::info),
      "Log Level")("HTTPProxy", po::value<string>(),
                   "HTTP Proxy")("Circles", po::value<string>(),
                                 "CircleIDs to Download, Seperated by comma")(
      "DownloadCount", po::value<int>()->default_value(16),
      "Download Thread Count")(
      "QueryCount", po::value<int>()->default_value(16), "Query Thread Count")(
      "Filters", po::value<string>(), "Type Filters Seperated by comma")(
      "Follows", po::value<string>(),
      "Download Liked Works of these UIDs, Seperated by comma")(
      "Groups", po::value<string>(),
      "Download Groups By GID, Seperated by comma")(
      "SaveBase", po::value<string>(),
      "SaveBase Directory Path")("Searches", po::value<string>(),
                                 "Keywords to Search, Seperated by comma")(
      "Tags", po::value<string>(), "Keywords to Download, Seperated by comma")(
      "UseCache", "Enable usage of cached WorkInfo")(
      "Users", po::value<string>(),
      "Download Works From these UIDs, Seperated by comma")(
      "Verify", "Verify everything in the database is downloaded")(
      "Works", po::value<string>(),
      "Download Liked Works of these WorkIDs, Seperated by comma")(
      "Events", po::value<string>(),
      "Download Works From Those EventIDs, Seperated by comma")(
      "aria2", po::value<string>(),
      "Aria2 RPC Server. Format: RPCURL[@RPCTOKEN]")(
      "email", po::value<string>(), "BCY Account email")(
      "password", po::value<string>(), "BCY Account password")(
      "DBPath", po::value<string>()->default_value(""), "BCY Database Path")(
      "UID", po::value<string>()->default_value(""), "BCY UID")(
      "sessionKey", po::value<string>()->default_value(""), "BCY sessionKey");
  try {
    po::store(
        po::command_line_parser(argc, argv).options(desc).positional(pos).run(),
        vm);
    po::notify(vm);
  } catch (std::exception &exp) {
    cout << "Parsing Option Error:" << exp.what() << endl;
    cout << desc << endl;
    return -1;
  }
  auto fmtTimeStamp = expr::format_date_time<posix_time::ptime>(
      "TimeStamp", "%Y-%m-%d %H:%M:%S");
  auto fmtSeverity = expr::attr<logging::trivial::severity_level>("Severity");
  log::formatter logFmt = logging::expressions::format("[%1%] [%2%] %3%") %
                          fmtTimeStamp % fmtSeverity % expr::smessage;
  auto consoleSink = log::add_console_log(std::clog);
  consoleSink->set_formatter(logFmt);
  logging::core::get()->set_filter(
      logging::trivial::severity >=
      vm["log-level"].as<logging::trivial::severity_level>());

  if (vm.count("help")) {
    cout << desc << endl;
    return 0;
  }
  if (vm["config"].as<string>() != "") {
    string JSONPath = vm["config"].as<string>();
    ifstream JSONStream(JSONPath);
    if (JSONStream.good()) {
      string JSONStr;
      JSONStream.seekg(0, ios::end);
      JSONStr.reserve(JSONStream.tellg());
      JSONStream.seekg(0, ios::beg);
      JSONStr.assign((istreambuf_iterator<char>(JSONStream)),
                     istreambuf_iterator<char>());
      web::json::value conf = web::json::value::parse(JSONStr);
      // Implement Options Handling.
      // Note Options inside the JSON have higher priority
      if (!conf.has_field("DownloadCount")) {
        config["DownloadCount"] = vm["DownloadCount"].as<int>();
      } else {
        config["DownloadCount"] = conf["DownloadCount"].as_integer();
      }
      if (!conf.has_field("DBPath")) {
        config["DBPath"] = web::json::value(vm["DBPath"].as<string>());
      } else {
        config["DBPath"] = conf["DBPath"];
      }
      if (!conf.has_field("QueryCount")) {
        config["QueryCount"] = vm["QueryCount"].as<int>();
      } else {
        config["QueryCount"] = conf["QueryCount"].as_integer();
      }
      if (conf.has_field("HTTPProxy")) {
        config["HTTPProxy"] = conf["HTTPProxy"];
      } else if (vm.count("HTTPProxy") && vm["HTTPProxy"].as<string>() != "") {
        config["HTTPProxy"] = web::json::value(vm["HTTPProxy"].as<string>());
      }
      for (string K : {"Circles", "Filters", "Follows", "Groups", "Searches",
                       "Tags", "Users", "Works", "Events"}) {
        set<string> items;
        if (conf.has_field(K)) {
          for (auto item : conf[K].as_array()) {
            items.insert(item.as_string());
          }
        }
        if (vm.count(K)) {
          string arg = vm[K].as<string>();
          vector<string> results;
          boost::split(results, arg, [](char c) { return c == ','; });
          for (string foo : results) {
            items.insert(foo);
          }
        }
        vector<web::json::value> j;
        for (string item : items) {
          j.push_back(web::json::value(item));
        }
        config[K] = web::json::value::array(j);
      }
      for (string K : {"Verify", "UseCache", "DownloadVideo"}) {
        if (conf.has_field(K) == false) {
          if (vm.count(K)) {
            config[K] = web::json::value::boolean(true);
          } else {
            config[K] = web::json::value::boolean(false);
          }
        } else {
          config[K] = conf[K];
        }
      }
      for (string K : {"email", "password", "SaveBase"}) {
        if (vm.count(K)) {
          config[K] = web::json::value(vm[K].as<string>());
        } else if (conf.has_field(K)) {
          config[K] = conf[K];
        }
      }

      if (conf.has_field("aria2") == false) {
        if (vm.count("aria2")) {
          string arg = vm["aria2"].as<string>();
          config["aria2"] = web::json::value();
          if (arg.find_first_of("@") == string::npos) {
            config["aria2"]["RPCServer"] = web::json::value(arg);
          } else {
            size_t pos = arg.find_first_of("@");
            config["aria2"]["RPCServer"] = web::json::value(arg.substr(0, pos));
            config["aria2"]["secret"] = web::json::value(arg.substr(pos));
          }
        }
      } else {
        config["aria2"] = conf["aria2"];
      }
      int queryThreadCount = config["QueryCount"].as_integer();
      int downloadThreadCount = config["DownloadCount"].as_integer();
      if (conf.has_field("SaveBase") &&
          conf.at("SaveBase").is_null() == false) {
        config["SaveBase"] = conf["SaveBase"];
      } else {
        if (vm.count("SaveBase")) {
          config["SaveBase"] = web::json::value(vm["SaveBase"].as<string>());
        } else {
          cout << "SaveBase Not Specified. Default to ~/BCY/" << endl;
          config["SaveBase"] = web::json::value(BCY::expand_user("~/BCY/"));
        }
      }

      if (!config.has_field("DBPath")) {
        bfs::path dir(config["SaveBase"].as_string());
        bfs::path file("BCYInfo.db");
        bfs::path full_path = dir / file;
        config["DBPath"] = web::json::value(full_path.string());
      }
      try {

        init(config["SaveBase"].as_string(), queryThreadCount,
             downloadThreadCount, config["DBPath"].as_string());
        cout << "Initialized Downloader at: " << config["SaveBase"].as_string()
             << endl;
      } catch (const SQLite::Exception &ex) {
        cout << "Database Error:" << ex.getErrorStr() << endl
             << "Error Code:" << ex.getErrorCode() << endl
             << "Extended Error Code:" << ex.getExtendedErrorCode() << endl;
        abort();
      }
      signal(SIGINT, cleanup);
      atexit(cleanup2);

      if (config.has_field("UseCache") &&
          config.at("UseCache").is_null() == false) {
        DU->useCachedInfo = config["UseCache"].as_bool();
      }
      if (config.has_field("HTTPProxy") &&
          config.at("HTTPProxy").is_null() == false) {
        DU->core.proxy = config["HTTPProxy"].as_string();
      }
      if (config.has_field("Types") && config.at("Types").is_null() == false) {
        for (auto F : config["Types"].as_array()) {
          DU->addTypeFilter(F.as_string());
        }
      }
      if (config.has_field("aria2") && config.at("aria2").is_null() == false) {
        if (config.at("aria2").has_field("secret")) {
          DU->secret = config["aria2"]["secret"].as_string();
        }
        DU->RPCServer = config["aria2"]["RPCServer"].as_string();
      }
      if (config.has_field("DownloadVideo") &&
          config.at("DownloadVideo").is_null() == false) {
        bool flag = config["DownloadVideo"].as_bool();
        DU->downloadVideo = flag;
      }
      if (config.has_field("email") && config.has_field("password") &&
          config.at("email").is_null() == false &&
          config.at("password").is_null() == false) {
        login(config["email"].as_string(), config["password"].as_string());
      } else if (config.has_field("UID") && config.has_field("sessionKey") &&
                 config.at("UID").is_null() == false &&
                 config.at("sessionKey").is_null() == false) {
        loginWithSKey(config["UID"].as_string(),
                      config["sessionKey"].as_string());
      }
    } else {
      cout << "Failed to Open File at:" << JSONPath << "!" << endl;
    }
  }
  if (vm.count("i") || DU == nullptr) {
    Interactive();
  } else {
    JSONMode();
  }
  return 0;
}
