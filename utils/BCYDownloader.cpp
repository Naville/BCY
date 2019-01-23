#include "BCY/Base64.h"
#include "BCY/Core.hpp"
#include "BCY/DownloadUtils.hpp"
#include "BCY/Utils.hpp"
#include <algorithm>
#include <boost/algorithm/string.hpp>
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
chaiscript::ChaiScript engine;
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
static void process() { JSONMode(); }
static void aria2(string addr, string secret) {
  DU->RPCServer = addr;
  DU->secret = secret;
}
static void init(string path, int queryCnt, int downloadCnt) {
  if (DU == nullptr) {
    try {
      DU = new DownloadUtils(path, queryCnt, downloadCnt);
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
static void addtype(string typ) { DU->addTypeFilter(typ); }
static void user(string UID) { DU->downloadUser(UID); }
static void tag(string tag) { DU->downloadTag(tag); }
static void work(string workid) { DU->downloadWorkID(workid); }
static void item(string item_id) { DU->downloadItemID(item_id); }
static void proxy(string proxy) { DU->core.proxy = proxy; }
static void group(string gid) { DU->downloadGroupID(gid); };
static void cleanup() { DU->cleanup(); };
static void join() { DU->join(); };
static void login(string email, string password) {
  if (DU->core.UID == "") {
    web::json::value Res = DU->core.loginWithEmailAndPassword(email, password);
    if (!Res.is_null()) {
      Prefix = Res["data"]["uname"].as_string();
    } else {
      cout << "Login Failed" << endl;
    }
  } else {
    cout << "Already logged in as UID:" << DU->core.UID << endl;
  }
}
static void verifyUID(string UID, bool reverse) { DU->verifyUID(UID, reverse); }
static void verify(bool reverse) { DU->verify("", {}, reverse); }
static void forcequit() { kill(getpid(), 9); }
static void searchKW(string kw) { DU->downloadSearchKeyword(kw); }
static void verifyTag(string TagName, bool reverse) {
  DU->verifyTag(TagName, reverse);
}
static void block(string OPType, string arg) {
  boost::to_upper(OPType);
  if (OPType == "UID") {
    DU->filter->UIDList.push_back(web::json::value(arg));
  } else if (OPType == "WORK") {
    DU->filter->WorkList.push_back(web::json::value(arg));
  } else if (OPType == "TAG") {
    DU->filter->TagList.push_back(web::json::value(arg));
  } else if (OPType == "USERNAME") {
    DU->filter->UserNameList.push_back(web::json::value(arg));
  } else if (OPType == "TYPE") {
    DU->filter->TypeList.push_back(web::json::value(arg));
  } else {
    cout << "Unrecognized OPType" << endl;
  }
}
void JSONMode() {
  if (DU == nullptr) {
    cout << "You havn't initialize the downloader yet" << endl;
    return;
  }
  DU->downloadLiked();
  mt19937 mt_rand(time(0));
  vector<string> Tags;
  for (web::json::value j : config["Tags"].as_array()) {
    Tags.push_back(j.as_string());
  }
  shuffle(Tags.begin(), Tags.end(), mt_rand);
  for (string item : Tags) {
    DU->downloadTag(item);
  }

  vector<string> Searches;
  for (web::json::value j : config["Searches"].as_array()) {
    Searches.push_back(j.as_string());
  }
  shuffle(Searches.begin(), Searches.end(), mt_rand);
  for (string item : Searches) {
    DU->downloadSearchKeyword(item);
  }

  vector<string> Works;
  for (web::json::value j : config["Works"].as_array()) {
    Works.push_back(j.as_string());
  }
  shuffle(Works.begin(), Works.end(), mt_rand);
  for (string item : Works) {
    DU->downloadWorkID(item);
  }

  vector<string> Groups;
  for (web::json::value j : config["Groups"].as_array()) {
    Groups.push_back(j.as_string());
  }
  shuffle(Groups.begin(), Groups.end(), mt_rand);
  for (string item : Groups) {
    DU->downloadGroupID(item);
  }

  vector<string> Follows;
  for (web::json::value j : config["Follows"].as_array()) {
    Follows.push_back(j.as_string());
  }
  shuffle(Follows.begin(), Follows.end(), mt_rand);
  for (string item : Follows) {
    DU->downloadUserLiked(item);
  }
  vector<string> Users;
  for (web::json::value j : config["Users"].as_array()) {
    Users.push_back(j.as_string());
  }
  shuffle(Users.begin(), Users.end(), mt_rand);
  for (string item : Users) {
    DU->downloadUser(item);
  }

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

  string command;
  while (1) {
    cout << Prefix << ":$";
    getline(cin, command);
    if (!cin.good()) {
      break;
    }
    try {
      engine.eval(command);
    } catch (const std::exception &exc) {
      cout << "Exception:" << exc.what() << endl;
    }
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
static bool containsTag(web::json::value inf,string key){
  if(inf.has_field(key)){
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
      "Compress", "Enable Downloading Compressed Images")(
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
      "aria2", po::value<string>(),
      "Aria2 RPC Server. Format: RPCURL[@RPCTOKEN]")(
      "email", po::value<string>(), "BCY Account email")(
      "password", po::value<string>(), "BCY Account password")(
      "DBPath", po::value<string>()->default_value(""), "BCY Database Path");
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
    if (JSONStream.bad() == false) {
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
                       "Tags", "Users", "Works"}) {
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
      for (string K : {"Verify", "UseCache", "Compress"}) {
        if (conf.has_field(K)) {
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
      try {
        DU = new DownloadUtils(config["SaveBase"].as_string(), queryThreadCount,
                               downloadThreadCount,
                               config["DBPath"].as_string());
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

      if (config.has_field("Compress") &&
          config.at("Compress").is_null() == false) {
        bool flag = config["Compress"].as_bool();
        DU->allowCompressed = flag;
      }
      if (config.has_field("email") && config.has_field("password") &&
          config.at("email").is_null() == false &&
          config.at("password").is_null() == false) {
        cout << "Logging in..." << endl;
        web::json::value Res = DU->core.loginWithEmailAndPassword(
            config["email"].as_string(), config["password"].as_string());
        if (!Res.is_null()) {
          Prefix = Res["data"]["uname"].as_string();
          cout << "Logged in as : " << Prefix << endl;
        } else {
          cout << "Login Failed" << endl;
        }
      }
    }
    else{
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
