#include "Base64.h"
#include "Core.hpp"
#include "DownloadUtils.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <fstream>
#include <random>
using namespace BCY;
using namespace std::placeholders;
using namespace std;
using namespace boost;

DownloadUtils *DU = nullptr;

static string expand_user(string path) {
  if (not path.empty() and path[0] == '~') {
    assert(path.size() == 1 or path[1] == '/'); // or other error handling
    char const *home = getenv("HOME");
    if (home or ((home = getenv("USERPROFILE")))) {
      path.replace(0, 1, home);
    } else {
      char const *hdrive = getenv("HOMEDRIVE"), *hpath = getenv("HOMEPATH");
      assert(hdrive); // or other error handling
      assert(hpath);
      path.replace(0, 1, std::string(hdrive) + hpath);
    }
  }
  return path;
}

[[noreturn]] void cleanup(int sig) {
  delete DU;
  abort();
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
[[noreturn]] void Interactive(int argc, char **argv) {
  if(argc==3){
    string JSONPath = argv[1];
    ifstream JSONStream(JSONPath);
    string JSONStr;
    JSONStream.seekg(0, ios::end);
    JSONStr.reserve(JSONStream.tellg());
    JSONStream.seekg(0, ios::beg);
    JSONStr.assign((istreambuf_iterator<char>(JSONStream)),
                   istreambuf_iterator<char>());

    json j = json::parse(JSONStr);
    int queryThreadCount = 32;
    int downloadThreadCount = 64;
    if (j.find("QueryCount") != j.end()) {
      queryThreadCount = j["QueryCount"];
    }
    if (j.find("DownloadCount") != j.end()) {
      downloadThreadCount = j["DownloadCount"];
    }
    DU = new DownloadUtils(j["SaveBase"], queryThreadCount, downloadThreadCount);
    signal(SIGINT, cleanup);

    if (j.find("UseCache") != j.end()) {
      DU->useCachedInfo = j["UseCache"];
    }
    if (j.find("HTTPProxy") != j.end()) {
      DU->core.proxy = {{"http", j["HTTPProxy"]}, {"https", j["HTTPProxy"]}};
    }
    if (j.find("Filters") != j.end()) {
      for (string F : j["Filters"]) {
        DU->addFilter(F);
      }
    }

    if (j.find("aria2") != j.end()) {
      if (j["aria2"].find("secret") != j["aria2"].end()) {
        DU->secret = j["aria2"]["secret"];
      }
      DU->RPCServer = j["aria2"]["RPCServer"];
    }

    if (j.find("Compress") != j.end()) {
      bool flag = j["Compress"];
      DU->allowCompressed = flag;
    }

    if (j.find("email") != j.end() && j.find("password") != j.end()) {
      cout << "Logging in..." << endl;
      DU->core.loginWithEmailAndPassword(j["email"], j["password"]);
      cout << "Logged in as UID:" << DU->core.UID << endl;
    }
  }

  string Prefix = "BCYDownloader"; // After Login we replace this with UserName
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
      if (commands[0] == "help") {
        cout << "Commands List:" << endl
             << "Login EMAIL PASSWORD:" << endl
             << "\tLogin" << endl
             << "Init /SAVE/PATH/ [QueryThreadCount] [DownloadThreadCount]:"
             << endl
             << "\tInitialize the Downloader" << endl
             << "Quit:" << endl
             << "\tCancel Pending Queries And Quit" << endl
             << "ForceQuit:" << endl
             << "\tForce Quit" << endl
             << "AddFilter DownloadFilterName:" << endl
             << "\tApply the DownloadFilter to Supported Operations" << endl
             << "User UID:" << endl
             << "\tDownload Original Works By UID" << endl
             << "Work WorkID:" << endl
             << "\tDownload WorkCircle By UID" << endl
             << "Group GroupID:" << endl
             << "\tDownload GroupID" << endl
             << "Search Keyword:" << endl
             << "\tSearch And Download Works By Keyword" << endl
             << "Tag TagName:" << endl
             << "\tDownload Works By TagName" << endl
             << "Item item_id:" << endl
             << "\tDownload item_id" << endl
             << "liked [UID]:" << endl
             << "\tDownload UID's liked works if UID is provided, else "
                "download current user's liked works"<<endl
             << "Proxy ProxyURL:" << endl
             << "\tSet ProxyURL" << endl
             << "Aria2 URL [secret]" << endl
             << "\t Set Aria2's RPC URL And SecretKey" << endl;
        continue;
      } else if (commands[0] == "quit") {
        if (DU != nullptr) {
          delete DU;
        }
        exit(0);
      } else if (commands[0] == "liked") {
        if (DU != nullptr) {
          if (commands.size() == 2) {
            DU->downloadUserLiked(commands[1]);
          } else {
            DU->downloadLiked();
          }
        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "work") {
        if (DU != nullptr) {
          if (commands.size() == 2) {
            DU->downloadWorkID(commands[1]);
          } else {
            cout << "Usage:Work WorkIDToDownload" << endl;
          }
        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "group") {
        if (DU != nullptr) {
          if (commands.size() == 2) {
            DU->downloadGroupID(commands[1]);
          } else {
            cout << "Usage:Group GroupIDToDownload" << endl;
          }
        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "item") {
        if (DU != nullptr) {
          if (commands.size() == 2) {
            DU->downloadItemID(commands[1]);
          } else {
            cout << "Usage:item item_id" << endl;
          }
        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "search") {
        if (DU != nullptr) {
          if (commands.size() == 2) {
            DU->downloadSearchKeyword(commands[1]);
          } else {
            cout << "Usage:search Keyword" << endl;
          }
        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }

      } else if (commands[0] == "tag") {
        if (DU != nullptr) {
          if (commands.size() == 2) {
            DU->downloadTag(commands[1]);
          } else {
            cout << "Usage:Tag TagNameToDownload" << endl;
          }
        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "user") {
        if (DU != nullptr) {
          if (commands.size() == 2) {
            DU->downloadUser(commands[1]);
          } else {
            cout << "Usage:user UIDToDownload" << endl;
          }
        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "forcequit") {
        kill(getpid(), 9);
      } else if (commands[0] == "addfilter") {
        if (DU != nullptr) {
          if (commands.size() != 2) {
            cout << "Usage:addfilter FilterName" << endl;
          } else {
            DU->addFilter(commands[1]);
          }
        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "proxy") {
        if (DU != nullptr) {
          if (commands.size() != 2) {
            cout << "Usage:proxy URL" << endl;
          } else {
            DU->core.proxy = {{"http", commands[1]}, {"https", commands[1]}};
          }

        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "aria2") {
        if (DU != nullptr) {
          if (commands.size() < 2) {
            cout << "Usage:aria2 URL [secret]" << endl;
          } else {

            DU->RPCServer = commands[1];
            if (commands.size() > 2) {
              DU->secret = commands[2];
            }
          }

        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "login") {
        if (DU != nullptr) {
          if (DU->core.UID == "") {
            if (commands.size() != 3) {
              cout << "Usage:login email password" << endl;
            } else {
              json Res =
                  DU->core.loginWithEmailAndPassword(commands[1], commands[2]);
              if (!Res.is_null()) {
                Prefix = Res["data"]["uname"];
              } else {
                cout << "Login Failed" << endl;
              }
            }
          } else {
            cout << "Already logged in as UID:" << DU->core.UID << endl;
          }
        } else {
          cout << "You havn't initialize the downloader yet" << endl;
        }
      } else if (commands[0] == "init") {
        if (DU == nullptr) {
          if (commands.size() < 2) {
            cout << "Usage:init /SAVE/PATH/ [QueryThreadCount] "
                    "[DownloadThreadCount]"
                 << endl;
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
        continue;
      }

      else {
        cout << "Unknown command:" << command << ". Use help for command list"
             << endl;
      }
    } catch (const std::exception &exc) {
      cout << "Exception:" << exc.what() << endl;
    }
  }
  exit(-1);
}
[[noreturn]] void JSONMode(int argc, char **argv) {
  string JSONPath = argv[1];
  ifstream JSONStream(JSONPath);
  string JSONStr;
  JSONStream.seekg(0, ios::end);
  JSONStr.reserve(JSONStream.tellg());
  JSONStream.seekg(0, ios::beg);
  JSONStr.assign((istreambuf_iterator<char>(JSONStream)),
                 istreambuf_iterator<char>());

  json j = json::parse(JSONStr);
  int queryThreadCount = 32;
  int downloadThreadCount = 64;
  if (j.find("QueryCount") != j.end()) {
    queryThreadCount = j["QueryCount"];
  }
  if (j.find("DownloadCount") != j.end()) {
    downloadThreadCount = j["DownloadCount"];
  }
  DU = new DownloadUtils(j["SaveBase"], queryThreadCount, downloadThreadCount);
  signal(SIGINT, cleanup);

  if (j.find("UseCache") != j.end()) {
    DU->useCachedInfo = j["UseCache"];
  }
  if (j.find("HTTPProxy") != j.end()) {
    DU->core.proxy = {{"http", j["HTTPProxy"]}, {"https", j["HTTPProxy"]}};
  }
  if (j.find("Filters") != j.end()) {
    for (string F : j["Filters"]) {
      DU->addFilter(F);
    }
  }

  if (j.find("aria2") != j.end()) {
    if (j["aria2"].find("secret") != j["aria2"].end()) {
      DU->secret = j["aria2"]["secret"];
    }
    DU->RPCServer = j["aria2"]["RPCServer"];
  }

  if (j.find("Compress") != j.end()) {
    bool flag = j["Compress"];
    DU->allowCompressed = flag;
  }

  if (j.find("email") != j.end() && j.find("password") != j.end()) {
    cout << "Logging in..." << endl;
    DU->core.loginWithEmailAndPassword(j["email"], j["password"]);
    cout << "Logged in as UID:" << DU->core.UID << endl;
    DU->downloadLiked();
  }
  mt19937 mt_rand(time(0));
  vector<string> Tags = j["Tags"];
  shuffle(Tags.begin(),Tags.end(),mt_rand);
  for (string item : Tags) {
    DU->downloadTag(item);
  }

  vector<string> Searches = j["Searches"];
  shuffle(Searches.begin(), Searches.end(),mt_rand);
  for (string item : Searches) {
    DU->downloadSearchKeyword(item);
  }

  vector<string> Works = j["Works"];
  shuffle(Works.begin(), Works.end(),mt_rand);
  for (string item : Works) {
    DU->downloadWorkID(item);
  }

  vector<string> Groups = j["Groups"];
  shuffle(Groups.begin(), Groups.end(),mt_rand);
  for (string item : Groups) {
    DU->downloadGroupID(item);
  }

  vector<string> Follows = j["Follows"];
  shuffle(Follows.begin(), Follows.end(),mt_rand);
  for (string item : Follows) {
    DU->downloadUserLiked(item);
  }
  vector<string> Users = j["Users"];
  shuffle(Users.begin(), Users.end(),mt_rand);
  for (string item : Users) {
    DU->downloadUser(item);
  }

  DU->downloadTimeline();
  DU->join();
  delete DU;
  exit(0);
}
int main(int argc, char **argv) {
  if (argc == 2) {
    JSONMode(argc, argv);
    __builtin_unreachable();
  } else if (argc == 3) {
    Interactive(argc, argv);
    __builtin_unreachable();
  }
  else{
    cout<<"Usage"<<endl<<argv[0]<<" /PATH/TO/JSON for non-interactive console"<<endl
    <<"Appending anything as the third argument setups up a interactive console with the JSON"<<endl
    <<argv[0]<<" with no arguments starts a clear interactive console"<<endl;
  }
}
