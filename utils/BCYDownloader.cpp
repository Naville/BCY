#include "Base64.h"
#include "Core.hpp"
#include "DownloadUtils.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <boost/log/support/date_time.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup.hpp>
#include <fstream>
#include <random>
#include "json.hpp"
using namespace BCY;
using namespace std::placeholders;
using namespace std;
using namespace boost;
using namespace nlohmann;
namespace po = boost::program_options;
namespace logging=boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

static DownloadUtils *DU = nullptr;
static po::variables_map vm;
static po::positional_options_description pos;
static json config;
static string Prefix = "BCYDownloader"; // After Login we replace this with UserName

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
void cleanup(int sig) {
    if(DU!=nullptr){
        delete DU;
        DU = nullptr;
    }
    kill(getpid(),SIGKILL);
}
void cleanup2() {
    if(DU!=nullptr){
        delete DU;
        DU = nullptr;
    }
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
        if(ver){
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
                "download current user's liked works"
                << endl
                << "Proxy ProxyURL:" << endl
                << "\tSet ProxyURL" << endl
                << "Aria2 URL [secret]" << endl
                << "\t Set Aria2's RPC URL And SecretKey.Pass no option to "
                "disable Aria2 "
                << endl
                << "verify" << endl
                << "\t Verify All Cached Info" << endl
                << "join" << endl
                << "\t Join all working threads" << endl
                << "process" << endl
                << "\t Fallback to JSON Processing Mode" << endl
                << "cleanup" << endl
                << "\t Cleanup Download Folder based on Filters" << endl
                << "verifyUID UID" << endl
                << "\t Verify Work By UID" << endl
                << "verifyTag TagName" << endl
                << "\t Verify Work By TagName" << endl;
                continue;
            } else if (commands[0] == "quit") {
                if (DU != nullptr) {
                    delete DU;
                    DU=nullptr;
                }
                exit(0);
            } else if (commands[0] == "process") {
                JSONMode();
            }
            else if (commands[0] == "cleanup") {
                DU->cleanup();
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
            } else if (commands[0] == "verify") {
                if (DU != nullptr) {
                    DU->verify();
                }
                else{
                    cout << "You havn't initialize the downloader yet" << endl;
                }
            }else if (commands[0] == "verifyuid") {
                if (DU != nullptr) {
                    if (commands.size() == 2) {
                        DU->verifyUID(commands[1]);
                    } else {
                        cout << "Usage:verifyUID UID" << endl;
                    }
                }
                else{
                    cout << "You havn't initialize the downloader yet" << endl;
                }
            }
            else if (commands[0] == "verifytag") {
                if (DU != nullptr) {
                    if (commands.size() == 2) {
                        DU->verifyTag(commands[1]);
                    } else {
                        cout << "Usage:verifyTag TagName" << endl;
                    }
                }
                else{
                    cout << "You havn't initialize the downloader yet" << endl;
                }
            }
            else if (commands[0] == "join") {
                if (DU != nullptr) {
                    DU->join();
                }
                else{
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
                        cout << "Disabling Aria2 And Fallback to builtin Downloader"
                        << endl;
                        DU->RPCServer = "";
                        DU->secret = "";
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
}
std::istream& operator>>(std::istream& in, logging::trivial::severity_level& sl)
{
    std::string token;
    in >> token;
    transform(token.begin(), token.end(), token.begin(), ::tolower);
    if (token == "info"){
        sl=boost::log::trivial::info;
    }
    else if (token == "trace"){
        sl=boost::log::trivial::trace;
    }
    else if (token == "debug"){
        sl=boost::log::trivial::debug;
    }
    else if (token == "warning"){
        sl=boost::log::trivial::warning;
    }
    else if (token == "error"){
        sl=boost::log::trivial::error;
    }
    else if (token == "fatal"){
        sl=boost::log::trivial::fatal;
    }
    else{
        in.setstate(std::ios_base::failbit);
    }
    return in;
}
int main(int argc, char **argv) {
    logging::add_common_attributes();
    po::options_description desc("BCYDownloader Options");
    desc.add_options()
    ("help", "Print Usage")
    ("config", po::value<string>()->default_value(""),"Initialize Downloader using JSON at provided path")
    ("i", "Interactive Console")
    ("log-level",po::value<logging::trivial::severity_level>()->default_value(logging::trivial::info),"Log Level")
    ;
    try {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(pos).run(), vm);
        po::notify(vm);
    } catch (std::exception &exp) {
        cout << "Parsing Option Error:" << exp.what() << endl;
        cout << desc << endl;
        return -1;
    }
    auto fmtTimeStamp = expr::
    format_date_time<posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S");
    auto fmtSeverity = expr::
    attr<logging::trivial::severity_level>("Severity");
    log::formatter logFmt =
    logging::expressions::format("[%1%] [%2%] %3%")
    % fmtTimeStamp % fmtSeverity % expr::smessage;
    auto consoleSink = log::add_console_log(std::clog);
    consoleSink->set_formatter(logFmt);
    logging::core::get()->set_filter(logging::trivial::severity>=vm["log-level"].as<logging::trivial::severity_level>());
    
    if (vm.count("help")) {
        cout << desc << endl;
        return 0;
    }
    if (vm["config"].as<string>() != "") {
        string JSONPath = vm["config"].as<string>();
        ifstream JSONStream(JSONPath);
        if (JSONStream.bad()) {
            cout << "Failed to Open File!" << endl;
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
        if (config.find("Filters") != config.end()) {
            for (string F : config["Filters"]) {
                DU->addFilter(F);
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
            json Res =
            DU->core.loginWithEmailAndPassword(config["email"], config["password"]);
            if (!Res.is_null()) {
                Prefix = Res["data"]["uname"];
                cout<< "Logged in as : "<<Prefix<<endl;
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
