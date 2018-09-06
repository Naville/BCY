#include "Core.hpp"
#include "Base64.h"
#include "Utils.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/regex.hpp>
#include <ctime>
#include <curl/curl.h>
using namespace std;
using namespace CryptoPP;
using namespace cpr;
using namespace boost;
#define BCY_KEY "com_banciyuan_AI"
static const string APIBase = "https://api.bcy.net/";
#warning Add Remaining Video CDN URLs and update CDN choosing Algorithm
static const vector<string> VideoCDNURLs = {
    "https://ib.365yg.com/video/urls/v/1/toutiao/mp4"};
namespace BCY {
Core::Core() {
  string alp = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  Params = Parameters{{"version_code", "4.3.2"},
                      {"mix_mode", "1"},
                      {"account_sdk_source", "app"},
                      {"language", "en-US"},
                      {"channel", "App Store"},
                      {"resolution", "2880*1800"},
                      {"aid", "1250"},
                      {"screen_width", "2880"},
                      {"os_api", "18"},
                      {"ac", "WIFI"},
                      {"os_version", "16.0.0"},
                      {"device_platform", "iphone"},
                      {"device_type", "iPhone14,5"},
                      {"vid", ""},
                      {"device_id", generateRandomString("1234567890", 11)},
                      {"openudid", generateRandomString(alp, 40)},
                      {"idfa", ""}};
  Sess.SetHeader(Header{
      {"User-Agent",
       "bcy 4.3.2 rv:4.3.2.6146 (iPad; iPhone OS 9.3.3; en_US) Cronet"}});
  errorHandler = [](Error err, string msg) {
    cout << msg << ":" << err.message << endl;
  };
}
string Core::EncryptData(string Input) {
  ECB_Mode<AES>::Encryption crypto;
  crypto.SetKey((const unsigned char *)BCY_KEY, strlen(BCY_KEY));
  string output;
  StringSource(
      Input, true,
      new StreamTransformationFilter(crypto, new StringSink(output),
                                     StreamTransformationFilter::PKCS_PADDING));
  return output;
}
Response Core::GET(string URL, json Para, Parameters Par) {
  vector<Pair> payloads;
  for (json::iterator it = Para.begin(); it != Para.end(); ++it) {
    string K = it.key();
    string V = it.value();
    payloads.push_back(Pair(K, V));
  }
  Payload p(payloads.begin(), payloads.end());
  for (auto i = 0; i < retry; i++) {
    lock_guard<mutex> guard(sessionLock);
    Sess.SetTimeout(Timeout(timeout));
    Sess.SetUrl(URL);
    Sess.SetPayload(p);
    Sess.SetProxies(proxy);
    if (Par.content != "") {
      Sess.SetParameters(Par);
    } else {
      Sess.SetParameters(Params);
    }
    Response tmp = Sess.Get();
    if (!tmp.error) {
      return tmp;
    }
  }
  lock_guard<mutex> guard(sessionLock);
  Sess.SetUrl(URL);
  if (Par.content != "") {
    Sess.SetParameters(Par);
  } else {
    Sess.SetParameters(Params);
  }
  Sess.SetTimeout(Timeout(timeout));
  Sess.SetPayload(p);
  Sess.SetProxies(proxy);
  return Sess.Get();
}
Response Core::POST(string URL, json Para, bool Auth, bool Encrypt,
                    Parameters Par) {
  if (URL.find(APIBase) == string::npos) {
    URL = APIBase + URL;
  }
  if (Auth == true) {
    if (Para.find("session_key") == Para.end() && sessionKey != "") {
      Para["session_key"] = sessionKey;
    }
  }
  if (Encrypt == true) {
    Para = EncryptParam(Para);
  } else {
    Para = mixHEXParam(Para);
  }
  vector<Pair> payloads;
  for (json::iterator it = Para.begin(); it != Para.end(); ++it) {
    string K = it.key();
    string V = it.value();
    payloads.push_back(Pair(K, V));
  }
  for (auto i = 0; i < retry; i++) {

    lock_guard<mutex> guard(sessionLock);
    Sess.SetUrl(URL);
    Sess.SetTimeout(Timeout(timeout));
    Payload p(payloads.begin(), payloads.end());
    Sess.SetPayload(p);
    if (Par.content != "") {
      Sess.SetParameters(Par);
    } else {
      Sess.SetParameters(Params);
    }
    Sess.SetProxies(proxy);
    Response tmp = Sess.Post();
    if (!tmp.error) {
      return tmp;
    }
  }
  lock_guard<mutex> guard(sessionLock);
  Sess.SetUrl(URL);
  Payload p(payloads.begin(), payloads.end());
  Sess.SetPayload(p);
  Sess.SetTimeout(Timeout(timeout));
  if (Par.content != "") {
    Sess.SetParameters(Par);
  } else {
    Sess.SetParameters(Params);
  }
  Sess.SetProxies(proxy);
  return Sess.Post();
}
json Core::mixHEXParam(json Params) {
  json j;
  for (json::iterator it = Params.begin(); it != Params.end(); ++it) {
    j[it.key()] = bda_hexMixedString(it.value());
  }
  return j;
}
json Core::ParamByCRC32URL(string FullURL) {
  // First a 16digit random number is generated and appended as param r=
  // So for example if baseurl is https://123456.com/item/detail/a , then random
  // number is generated and appended which makes the new URL
  // https://123456.com/item/detail/a?r=8329376462157075 Then a CRC 32 is
  // calculated among the part /item/detail/a?r=8329376462157075 Which in this
  // case is c8cbae96 , then we convert the hex to decimal representation ,
  // which is 3368791702 in this case Finally we append the crc32 value to the
  // URL as param s, so in this case the final URL is
  // https://123456.com/item/detail/a?r=8329376462157075&s=3368791702
  std::vector<std::string> results;
  stringstream tmp;
  split(results, FullURL, [](char c) { return c == '/'; });
  tmp << "/";
  for (int i = 3; i < results.size(); i++) {
    tmp << results[i] << "/";
  }
  string CRC32Candidate = tmp.str();
  CRC32Candidate = CRC32Candidate.substr(0, CRC32Candidate.length() - 1);
  string nonce = generateRandomString("1234567890", 16);
  CRC32Candidate = CRC32Candidate + "?r=" + nonce;
  CRC32 hash;
  unsigned int crc32_hash;
  CryptoPP::CRC32().CalculateDigest((byte *)&crc32_hash,
                                    (byte *)CRC32Candidate.c_str(),
                                    CRC32Candidate.size());
  json j;
  j["r"] = nonce;
  j["s"] = to_string(crc32_hash);
  return j;
}
json Core::videoInfo(string video_id) {
  // A few baseURLs, presumably CDN URLs.
  // The BaseURL is conjugated with the rest of the items. We should reverse the
  // algo to choose the correct CDN URL?
  string BaseURL = VideoCDNURLs[0];
  json CRC = ParamByCRC32URL(BaseURL + "/" + video_id);
  string nonce = CRC["r"];
  string crc = CRC["s"];
  Parameters P{{"r", nonce}, {"s", crc}};
  Response R = GET(BaseURL + "/" + video_id, json(), P);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  return json::parse(R.text);
}
json Core::EncryptParam(json Params) {
  string serialized = Params.dump();
  json j;
  string foo;
  Base64::Encode(EncryptData(serialized), &foo);
  j["data"] = foo;
  return j;
}
string Core::bda_hexMixedString(string input) {
  stringstream os;
  static const char key = 5;
  os << setfill('0') << setw(2);
  stringstream os2;
  for (int i = 0; i < input.length(); i++) {
    char Val = input[i] ^ key;
    os2 << hex << Val;
  }
  os << os2.str();
  return string_to_hex(os.str());
}
json Core::space_me() {
  json j;
  auto R = POST("api/space/me", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  json r = json::parse(R.text);
  return r;
}
json Core::loginWithEmailAndPassword(string email, string password) {
  json j;
  j["email"] = email;
  j["password"] = password;
  auto R = POST("passport/email/login/", j, false, false);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  json LoginResponse = json::parse(R.text);
  if (LoginResponse["message"] == "error") {
    string msg = LoginResponse["data"]["description"];
    errorHandler(Error(67 /*CURLE_LOGIN_DENIED*/, msg), msg);
    return json();
  }
  sessionKey = LoginResponse["data"]["session_key"];
  j = json();
  R = POST("api/token/doLogin", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  LoginResponse = json::parse(R.text);
  UID = LoginResponse["data"]["uid"];
  return LoginResponse;
}
json Core::user_detail(string UID) {
  json j;
  j["uid"] = UID;
  auto R = POST("api/token/doLogin", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  return json::parse(R.text);
}
json Core::image_postCover(string item_id, string type) {
  json j;
  j["id"] = item_id;
  j["type"] = type;
  auto R = POST("api/image/postCover/", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  return json::parse(R.text);
}
bool Core::user_follow(string uid, bool isFollow) {
  json j;
  j["uid"] = uid;
  if (isFollow) {
    j["type"] = "dofollow";
  } else {
    j["type"] = "unfollow";
  }
  auto R = POST("api/user/follow", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return false;
  }
  json r = json::parse(R.text);
  return r["status"] == 1;
}
json Core::item_detail(string item_id, bool autoFollow) {
  json j;
  j["item_id"] = item_id;
  auto R = POST("api/item/detail/", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  json r = json::parse(R.text);
  if (r["status"] == 4010 && autoFollow) {
    // Need to Follow
    string UID = r["data"]["profile"]["uid"];
    user_follow(UID, true);
    R = POST("api/item/detail/", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return json();
    }
    r = json::parse(R.text);
    user_follow(UID, false);
  }
  return r;
}
bool Core::item_doPostLike(string item_id) {
  json j;
  j["item_id"] = item_id;
  auto R = POST("api/item/doPostLike", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return false;
  }
  json r = json::parse(R.text);
  return r["status"] == 1;
}
bool Core::item_cancelPostLike(string item_id) {
  json j;
  j["item_id"] = item_id;
  auto R = POST("api/item/cancelPostLike", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return false;
  }
  json r = json::parse(R.text);
  return r["status"] == 1;
}
json Core::tag_status(string TagName) {
  json j;
  j["name"] = TagName;
  auto R = POST("api/tag/status", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  json r = json::parse(R.text);
  return r;
}
json Core::core_status(string WorkID) {
  json j;
  j["wid"] = WorkID;
  auto R = POST("api/core/status", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  json r = json::parse(R.text);
  return r;
}
json Core::circle_filterlist(string circle_id, CircleType circle_type,
                             string circle_name) {
  json j;
  j["circle_id"] = circle_id;
  switch (circle_type) {
  case CircleType::Tag: {
    j["circle_type"] = "tag";
    break;
  }
  case CircleType::Work: {
    j["circle_type"] = "work";
    break;
  }
  default: { throw invalid_argument("Invalid Circle Type!"); }
  }
  j["circle_name"] = circle_name;
  auto R = POST("apiv2/circle/filterlist/", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  json r = json::parse(R.text);
  return r;
}
vector<json> Core::search_item_bytag(list<string> TagNames, PType ptype,
                                     BCYListIteratorCallback callback) {
  vector<json> ret;
  int p = 0;
  json j;
  if (ptype != PType::Undef) {
    j["ptype"] = ptype;
  }
  j["tag_list"] = TagNames;
  while (true) {
    j["p"] = p;
    p++;
    auto R = POST("apiv2/search/item/bytag/", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"]["ItemList"];
    if (data.size() == 0) {
      return ret;
    }
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }

  return ret;
}
json Core::group_detail(string GID) {
  json j;
  j["gid"] = GID;
  auto R = POST("api/group/detail/", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  json r = json::parse(R.text);
  return r;
}
vector<json> Core::circle_itemhotworks(string circle_id,
                                       BCYListIteratorCallback callback) {
  vector<json> ret;
  int since = 0;
  json j;
  j["grid_type"] = "timeline";
  j["id"] = circle_id;
  while (true) {
    if (since == 0) {
      j["since"] = since;
    } else {
      j["since"] = "rec:" + to_string(since);
    }
    since++;
    auto R = POST("apiv2/circle/itemhotworks/", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"]["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
}
json Core::circle_itemhottags(string item_id) {
  json j;
  j["item_id"] = item_id;
  auto R = POST("apiv2/circle/itemhottags/", j, true, true);
  if (R.error) {
    errorHandler(R.error, __func__);
    return json();
  }
  json r = json::parse(R.text);
  return r;
}
vector<json> Core::circle_itemrecenttags(string TagName, string Filter,
                                         BCYListIteratorCallback callback) {
  vector<json> ret;
  string since = "0";
  json j;
  j["grid_type"] = "timeline";
  j["filter"] = Filter;
  j["name"] = TagName;
  while (true) {
    j["since"] = since;
    auto R = POST("api/circle/itemRecentTags/", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"];
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
  return ret;
}
vector<json> Core::item_getReply(string item_id,
                                 BCYListIteratorCallback callback) {
  vector<json> ret;
  int p = 1;
  json j;
  j["item_id"] = item_id;
  while (true) {
    j["p"] = p;
    p++;
    auto R = POST("api/item/getReply", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
  return ret;
}
vector<json> Core::circle_itemRecentWorks(string WorkID,
                                          BCYListIteratorCallback callback) {
  vector<json> ret;
  string since = "";
  json j;
  j["grid_type"] = "timeline";
  j["id"] = WorkID;
  while (true) {
    j["since"] = since;
    auto R = POST("api/circle/itemRecentWorks", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"];
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
  return ret;
}
vector<json>
Core::timeline_getUserPostTimeLine(string UID,
                                   BCYListIteratorCallback callback) {
  vector<json> ret;
  string since = "0";
  json j;
  j["grid_type"] = "timeline";
  j["uid"] = UID;
  while (true) {
    j["since"] = since;
    auto R = POST("api/timeline/getUserPostTimeLine/", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"];
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
  return ret;
}

vector<json> Core::space_getUserLikeTimeLine(string UID,
                                             BCYListIteratorCallback callback) {
  vector<json> ret;
  string since = "0";
  json j;
  j["grid_type"] = "grid";
  j["uid"] = UID;
  while (true) {
    j["since"] = since;
    auto R = POST("api/space/getUserLikeTimeLine/", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"];
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
  return ret;
}
vector<json> Core::search(string keyword, SearchType type,
                          BCYListIteratorCallback callback) {
  vector<json> ret;
  int p = 1;
  string URL = "api/search/search";
  switch (type) {
  case SearchType::Content: {
    URL = URL + "Content/";
    break;
  }
  case SearchType::Works: {
    URL = URL + "Works/";
    break;
  }
  case SearchType::Tags: {
    URL = URL + "Tags/";
    break;
  }
  case SearchType::User: {
    URL = URL + "User/";
    break;
  }
  default: { throw invalid_argument("Invalid Search Type!"); }
  }
  json j;
  j["query"] = keyword;
  while (true) {
    j["p"] = p;
    p++;
    auto R = POST(URL, j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"]["results"];
    if (data.size() == 0) {
      return ret;
    }
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
    if (type == SearchType::Tags || type == SearchType::Works) {
#warning                                                                       \
    "Remove this check after dumbasses at ByteDance got their shit straight"
      return ret;
    }
  }
  return ret;
}

vector<json> Core::group_listPosts(string GID,
                                   BCYListIteratorCallback callback) {
  vector<json> ret;
  int p = 1;
  json j;
  j["gid"] = GID;
  j["type"] = "ding";
  j["limit"] = 50;
  while (true) {
    j["p"] = p;
    p++;
    auto R = POST("api/group/listPosts", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
  return ret;
}
vector<json> Core::timeline_friendfeed(BCYListIteratorCallback callback) {
  vector<json> ret;
  string since = "0";
  json j;
  j["grid_type"] = "grid";
  j["uid"] = UID;
  while (true) {
    j["since"] = since;
    auto R = POST("apiv2/timeline/friendfeed/", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"];
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
  return ret;
}
vector<json> Core::item_favor_itemlist(BCYListIteratorCallback callback) {
  vector<json> ret;
  string since = "0";
  json j;
  j["grid_type"] = "grid";
  while (true) {
    j["since"] = since;
    auto R = POST("apiv2/item/favor/itemlist", j, true, true);
    if (R.error) {
      errorHandler(R.error, __func__);
      return ret;
    }
    json foo = json::parse(R.text);
    json data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"];
    for (json &ele : data) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
  return ret;
}
} // namespace BCY

__attribute__((constructor)) static void initialize_BCYCore() {
  // Reference: https://curl.haxx.se/libcurl/c/threadsafe.html
  curl_global_init(CURL_GLOBAL_ALL);
}
