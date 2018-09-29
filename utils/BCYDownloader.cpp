#include "BCY/Base64.h"
#include "BCY/Core.hpp"
#include "BCY/DownloadUtils.hpp"
#include "BCY/Utils.hpp"
#include "BCY/json.hpp"
#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <random>
using namespace BCY;
using namespace std::placeholders;
using namespace std;
using namespace boost;
using namespace nlohmann;
namespace po = boost::program_options;
namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

static DownloadUtils *DU = nullptr;
static po::variables_map vm;
static po::positional_options_description pos;
static json config;
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
void Init() {
  commHandle quitHandle = [=](vector<string>) -> void {
    if (DU != nullptr) {
      delete DU;
      DU = nullptr;
    }
    exit(0);
  };
  commHandle a2Handle = [=](vector<string> commands) {
    if (commands.size() < 2) {
      cout << "Disabling Aria2 And Fallback to builtin Downloader" << endl;
      DU->RPCServer = "";
      DU->secret = "";
    } else {

      DU->RPCServer = commands[1];
      if (commands.size() > 2) {
        DU->secret = commands[2];
      }
    }
  };
  commHandle processHandle = [=](vector<string> commands) { JSONMode(); };
  commHandle unlikeHandle = [=](vector<string> commands) {
    DU->unlikeCached();
  };
  commHandle likedHandle = [=](vector<string> commands) {
    if (commands.size() == 2) {
      DU->downloadUserLiked(commands[1]);
    } else {
      DU->downloadLiked();
    }
  };
  commHandle loginHandle = [=](vector<string> commands) {
    if (DU->core.UID == "") {
      json Res = DU->core.loginWithEmailAndPassword(commands[1], commands[2]);
      if (!Res.is_null()) {
        Prefix = Res["data"]["uname"];
      } else {
        cout << "Login Failed" << endl;
      }
    } else {
      cout << "Already logged in as UID:" << DU->core.UID << endl;
    }
  };
  commHandle initHandle = [=](vector<string> commands) {
    if (DU == nullptr) {
      if (commands.size() < 2) {
        cout << "Invalid Argument" << endl;
        return;
      } else {
        while (commands.size() < 4) {
          commands.push_back("16");
        }
        DU = new DownloadUtils(commands[1], stoi(commands[2]),
                               stoi(commands[3]));
        signal(SIGINT, cleanup);
      }
    } else {
      cout << "Already Initialized" << endl;
    }
  };
  commHandle userHandle = [=](vector<string> commands) {
    if (DU != nullptr) {
      if (commands.size() == 2) {
        DU->downloadUser(commands[1]);
      } else {
        cout << "Invalid Argument" << endl;
      }
    } else {
      cout << "You havn't initialize the downloader yet" << endl;
    }
  };
  commHandle tagHandle = [=](vector<string> commands) {
    if (commands.size() == 2) {
      DU->downloadTag(commands[1]);
    } else {
      cout << "Invalid Argument" << endl;
    }
  };
  commHandle addtypeHandle = [=](vector<string> commands) {
    if (commands.size() != 2) {
      cout << "Invalid Argument" << endl;
    } else {
      DU->addTypeFilter(commands[1]);
    }
  };
  commHandle itemHandle = [=](vector<string> commands) {
    if (commands.size() == 2) {
      DU->downloadItemID(commands[1]);
    } else {
      cout << "Invalid Argument" << endl;
    }
  };
  commHandle verifyUIDHandle = [=](vector<string> commands) {
    if (commands.size() == 2) {
      DU->verifyUID(commands[1]);
    } else if (commands.size() == 3) {
      string tmp = commands[2];
      transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
      if (tmp == "false") {
        DU->verifyUID(commands[1]);
      } else if (tmp == "true") {
        DU->verifyUID(commands[1], true);
      } else {
        cout << "Invalid Argument" << endl;
      }

    } else {
      cout << "Invalid Argument" << endl;
    }
  };
  commHandle verifyTagHandle = [=](vector<string> commands) {
    if (commands.size() == 2) {
      DU->verifyTag(commands[1]);
    } else if (commands.size() == 3) {
      string tmp = commands[2];
      transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
      if (tmp == "false") {
        DU->verifyTag(commands[1]);
      } else if (tmp == "true") {
        DU->verifyTag(commands[1], true);
      } else {
        cout << "Invalid Argument" << endl;
      }

    } else {
      cout << "Invalid Argument" << endl;
    }
  };
  commHandle workHandle = [=](vector<string> commands) {
    if (commands.size() == 2) {
      DU->downloadWorkID(commands[1]);
    } else {
      cout << "Invalid Argument" << endl;
    }
  };
  commHandle groupHandle = [=](vector<string> commands) {
    if (commands.size() == 2) {
      DU->downloadGroupID(commands[1]);
    } else {
      cout << "Invalid Argument" << endl;
    }
  };
  commHandle proxyHandle = [=](vector<string> commands) {
    if (commands.size() != 2) {
      cout << "Invalid Argument" << endl;
    } else {
      DU->core.proxy = {{"http", commands[1]}, {"https", commands[1]}};
    }
  };
  commHandle cleanupHandle = [=](vector<string> commands) { DU->cleanup(); };
  commHandle joinHandle = [=](vector<string> commands) { DU->join(); };
  commHandle verifyHandle = [=](vector<string> commands) {
    if (commands.size() == 1) {
      DU->verify();
    } else if (commands.size() == 2) {
      string tmp = commands[1];
      transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
      if (tmp == "false") {
        DU->verify();
      } else if (tmp == "true") {
        DU->verify("",{},true);
      } else {
        cout << "Invalid Argument" << endl;
      }
    } else {
      cout << "Invalid Argument" << endl;
    }
  };
  commHandle forcequitHandle = [=](vector<string> commands) {
    kill(getpid(), 9);
  };
  commHandle searchHandle = [=](vector<string> commands) {
    if (commands.size() == 2) {
      DU->downloadSearchKeyword(commands[1]);
    } else {
      cout << "Invalid Argument" << endl;
    }
  };
  commHandle helpHandle = [=](vector<string> commands) {
    if (commands.size() == 2 &&
        handlerMsgs.find(commands[1]) != handlerMsgs.end()) {
      cout << handlerMsgs[commands[1]] << endl;
    } else {
      for (map<string, string>::iterator it = handlerMsgs.begin();
           it != handlerMsgs.end(); it++) {
        cout << it->first // string (key)
             << endl
             << it->second // string's value
             << endl
             << endl;
      }
    }
  };
  commHandle itemDetailHandle = [=](vector<string> commands) {
    if (commands.size() != 2) {
      cout << "Invalid Argument" << endl;
      return;
    }
    cout << DU->core.item_detail(commands[1], false).dump() << endl;
  };
  commHandle followHandle = [=](vector<string> commands) {
    if (commands.size() != 2) {
      cout << "Invalid Argument" << endl;
      return;
    }
    cout << DU->core.user_follow(commands[1], true) << endl;
  };
  commHandle unfollowHandle = [=](vector<string> commands) {
    if (commands.size() != 2) {
      cout << "Invalid Argument" << endl;
      return;
    }
    cout << DU->core.user_follow(commands[1], false) << endl;
  };
  handlers["quit"] = quitHandle;
  handlerMsgs["quit"] = "Cancel Pending Queries And Quit";
  handlers["process"] = processHandle;
  handlerMsgs["process"] = "Fallback to JSON Processing Mode";
  handlers["aria2"] = a2Handle;
  handlerMsgs["aria2"] = "Usage:aria2 URL [secret] \n\t Set Aria2's RPC URL "
                         "And SecretKey.Pass no option to disable Aria2";
  handlers["unlike"] = unlikeHandle;
  handlerMsgs["unlike"] = "Unlike Cached Works";
  handlers["liked"] = likedHandle;
  handlerMsgs["liked"] =
      "Usage:liked [UID]\nDescription: Download UID's liked works if UID is "
      "provided, else download current user's liked works";
  handlers["login"] = loginHandle;
  handlerMsgs["login"] = "Usage:login EMAIL PASSWORD\nDescription: Login";
  handlers["init"] = initHandle;
  handlerMsgs["init"] =
      "Usage:init /SAVE/PATH/ [QueryThreadCount] "
      "[DownloadThreadCount]\nDescription: Initialize the Downloader";
  handlers["user"] = userHandle;
  handlerMsgs["user"] =
      "Usage:User UID\nDescription: Download Original Works By UID";
  handlers["tag"] = tagHandle;
  handlerMsgs["tag"] =
      "Usage:Tag TagName\nDescription: Download Works By TagName";
  handlers["addtype"] = addtypeHandle;
  handlerMsgs["addtype"] =
      "Usage:AddType DownloadTypeFilterName\nDescription: Apply the Download "
      "Type Filter to Supported Operations";
  handlers["item"] = itemHandle;
  handlerMsgs["item"] = "Usage:Item item_id\nDescription: Download item_id";
  handlers["cleanup"] = cleanupHandle;
  handlerMsgs["cleanup"] = "Usage:cleanup\nDescription: Cleanup";
  handlers["join"] = joinHandle;
  handlerMsgs["join"] = "Usage:Join\nDescription: Join all working threads";
  handlers["verifyuid"] = verifyUIDHandle;
  handlerMsgs["verifyuid"] =
      "Usage:verifyUID UID [ReverseVerify(true/false)]\nDescription: Verify Work By UID";
  handlers["verifytag"] = verifyTagHandle;
  handlerMsgs["verifytag"] =
      "Usage:verifyTag TagName [ReverseVerify(true/false)]\nDescription: Verify Work By TagName";
  handlers["verify"] = verifyHandle;
  handlerMsgs["verify"] = "Usage: verify [ReverseVerify(true/false)]\nDescription: Verify All Cached Info";
  handlers["work"] = workHandle;
  handlerMsgs["work"] =
      "Usage:Work WorkID\nDescription: Download WorkCircle By WorkID";
  handlers["group"] = groupHandle;
  handlerMsgs["group"] = "Usage:Group GroupID\nDescription: Download GroupID";
  handlers["proxy"] = proxyHandle;
  handlerMsgs["proxy"] = "Usage:Proxy ProxyURL\nDescription: Set ProxyURL";
  handlers["forcequit"] = forcequitHandle;
  handlerMsgs["forcequit"] = "Usage:forcequit\nDescription: Force Quit";
  handlers["search"] = searchHandle;
  handlerMsgs["search"] =
      "Usage:Search Keyword\nDescription: Search And Download Works By Keyword";
  handlers["help"] = helpHandle;
  handlerMsgs["help"] = "Usage:help\nDescription: Display This Help Message";
  handlers["detail"] = itemDetailHandle;
  handlerMsgs["detail"] =
      "Usage:detail item_id\nDescription: Display Detail Info of item_id";
  handlers["follow"] = followHandle;
  handlerMsgs["follow"] = "Usage:follow UID\nDescription: Follow UID";
  handlers["unfollow"] = unfollowHandle;
  handlerMsgs["unfollow"] = "Usage:unfollow UID\nDescription: Unfollow UID";
}

vector<string> ParseCommand(string Input) {
  algorithm::trim(Input);
  vector<string> components;
  string tmp = "";
  for (auto i = 0; i < Input.length(); i++) {
    if (Input.substr(i, 1) != " ") {
      tmp = tmp + Input.substr(i, 1);
    } else if (tmp != "") {
      components.push_back(tmp);
      tmp.clear();
    }
  }
  components.push_back(tmp);
  if (components.size() > 0) {
    transform(components[0].begin(), components[0].end(), components[0].begin(),
              ::tolower);
  }
  return components;
}
void JSONMode() {
  if (DU == nullptr) {
    cout << "You havn't initialize the downloader yet" << endl;
    return;
  }
  DU->downloadLiked();
  mt19937 mt_rand(time(0));
  vector<string> Tags = config["Tags"];
  shuffle(Tags.begin(), Tags.end(), mt_rand);
  for (string item : Tags) {
    DU->downloadTag(item);
  }

  vector<string> Searches = config["Searches"];
  shuffle(Searches.begin(), Searches.end(), mt_rand);
  for (string item : Searches) {
    DU->downloadSearchKeyword(item);
  }

  vector<string> Works = config["Works"];
  shuffle(Works.begin(), Works.end(), mt_rand);
  for (string item : Works) {
    DU->downloadWorkID(item);
  }

  vector<string> Groups = config["Groups"];
  shuffle(Groups.begin(), Groups.end(), mt_rand);
  for (string item : Groups) {
    DU->downloadGroupID(item);
  }

  vector<string> Follows = config["Follows"];
  shuffle(Follows.begin(), Follows.end(), mt_rand);
  for (string item : Follows) {
    DU->downloadUserLiked(item);
  }
  vector<string> Users = config["Users"];
  shuffle(Users.begin(), Users.end(), mt_rand);
  for (string item : Users) {
    DU->downloadUser(item);
  }

  DU->downloadTimeline();
  if (config.find("Verify") != config.end()) {
    bool ver = config["Verify"];
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
  string command;
  while (1) {
    cout << Prefix << ":$";
    getline(cin, command);
    if (!cin.good()) {
      break;
    }
    try {
      vector<string> commands = ParseCommand(command);
      if (commands[0] == "init") {
        handlers["init"](commands);
      } else if (handlers.find(commands[0]) != handlers.end()) {
        if (DU == nullptr) {
          cout << "You need to initialze the downloader first" << endl;
        } else {
          auto fn = handlers[commands[0]];
          fn(commands);
        }
      } else {
        handlers["help"](vector<string>());
      }
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
int main(int argc, char **argv) {
  Init();
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
      "Log Level");
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
    if (JSONStream.bad()) {
      cout << "Failed to Open File at:" << JSONPath << "!" << endl;
      exit(-1);
    }
    string JSONStr;
    JSONStream.seekg(0, ios::end);
    JSONStr.reserve(JSONStream.tellg());
    JSONStream.seekg(0, ios::beg);
    JSONStr.assign((istreambuf_iterator<char>(JSONStream)),
                   istreambuf_iterator<char>());
    config = json::parse(JSONStr);
    int queryThreadCount = 32;
    int downloadThreadCount = 64;
    if (config.find("QueryCount") != config.end()) {
      queryThreadCount = config["QueryCount"];
    }
    if (config.find("DownloadCount") != config.end()) {
      downloadThreadCount = config["DownloadCount"];
    }
    DU = new DownloadUtils(config["SaveBase"], queryThreadCount,
                           downloadThreadCount);
    signal(SIGINT, cleanup);
    atexit(cleanup2);

    if (config.find("UseCache") != config.end()) {
      DU->useCachedInfo = config["UseCache"];
    }
    if (config.find("HTTPProxy") != config.end()) {
      DU->core.proxy = {{"http", config["HTTPProxy"]},
                        {"https", config["HTTPProxy"]}};
    }
    if (config.find("Types") != config.end()) {
      for (string F : config["Types"]) {
        DU->addTypeFilter(F);
      }
    }

    if (config.find("aria2") != config.end()) {
      if (config["aria2"].find("secret") != config["aria2"].end()) {
        DU->secret = config["aria2"]["secret"];
      }
      DU->RPCServer = config["aria2"]["RPCServer"];
    }

    if (config.find("Compress") != config.end()) {
      bool flag = config["Compress"];
      DU->allowCompressed = flag;
    }
    if (config.find("email") != config.end() &&
        config.find("password") != config.end()) {
      cout << "Logging in..." << endl;
      json Res = DU->core.loginWithEmailAndPassword(config["email"],
                                                    config["password"]);
      if (!Res.is_null()) {
        Prefix = Res["data"]["uname"];
        cout << "Logged in as : " << Prefix << endl;
      } else {
        cout << "Login Failed" << endl;
      }
    }
  }
  if (vm.count("i") || DU == nullptr) {
    Interactive();
    return 0;
  } else {
    JSONMode();
  }

  return 0;
}
