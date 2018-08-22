#include "DownloadUtils.hpp"
#include <algorithm>
#include <boost/asio.hpp>
#include <fstream>
#include <random>
using namespace BCY;
using namespace std;
using namespace std::placeholders;
using namespace boost::asio;

DownloadUtils *DU = nullptr;

void cleanup(int sig) {
  delete DU;
  abort();
}
void cleanup2() {
  delete DU;
  abort();
}

int main(int argc, char **argv) {
  if (argc != 2) {
    cout << "Usage:" << endl << argv[0] << " /PATH/TO/CONFIG/JSON" << endl;
    exit(-1);
  }
  string JSONPath = argv[1];
  ifstream JSONStream(JSONPath);
  string JSONStr;
  vector<string> Filters;
  JSONStream.seekg(0, ios::end);
  JSONStr.reserve(JSONStream.tellg());
  JSONStream.seekg(0, ios::beg);
  JSONStr.assign((istreambuf_iterator<char>(JSONStream)),
                 istreambuf_iterator<char>());

  json j = json::parse(JSONStr);
  DU = new DownloadUtils(j["SaveBase"], 0, 0);
  signal(SIGINT, cleanup);
  if (j.find("HTTPProxy") != j.end()) {
    DU->core.proxy = {{"http", j["HTTPProxy"]}, {"https", j["HTTPProxy"]}};
  }
  cout << "Logging in..." << endl;
  DU->core.loginWithEmailAndPassword(j["email"], j["password"]);
  cout << "Logged in as UID:" << DU->core.UID << endl;
  vector<json> Liked = DU->core.space_getUserLikeTimeLine(DU->core.UID);
  cout << "Found " << Liked.size() << " Liked Works" << endl;
  thread_pool *t = new thread_pool(16);
  for (json j : Liked) {
    string item_id = j["item_detail"]["item_id"];
    boost::asio::post(*t, [=] {
      if (!DU->loadInfo(item_id).is_null()) {
        if (DU->core.item_cancelPostLike(item_id)) {
          cout << "Unlike item_id: " << item_id << " Success" << endl;
        } else {
          cout << "Unlike item_id: " << item_id << " Failed" << endl;
        }
      } else {
        cout << "item_id: " << item_id << " Not Found In Local Cache" << endl;
      }
    });
  }
  cout << "Joining Unlike Threads\n";
  t->join();
  delete DU;
  return 0;
}
