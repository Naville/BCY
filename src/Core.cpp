#include "BCY/Core.hpp"
#include "BCY/Base64.h"
#include "BCY/Utils.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <chrono>
#include <cpprest/http_client.h>
#include <cpprest/uri.h>
#include <ctime>
using namespace std;
using namespace CryptoPP;
using namespace boost;
using namespace web;               // Common features like URIs.
using namespace web::http;         // Common HTTP functionality
using namespace web::http::client; // HTTP client features
#define BCY_KEY "com_banciyuan_AI"
static const string APIBase = "https://api.bcy.net/";
#warning Add Remaining Video CDN URLs and update CDN choosing Algorithm
static const vector<string> VideoCDNURLs = {
    "https://ib.365yg.com/video/urls/v/1/toutiao/mp4"};
namespace BCY {
Core::Core() {
  string alp = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  string width = generateRandomString("1234567890", 4);
  string height = generateRandomString("1234567890", 4);
  Params = {{"version_code", "4.4.1"},
            {"mix_mode", "1"},
            {"account_sdk_source", "app"},
            {"language", "en-US"},
            {"channel", "App Store"},
            {"resolution", width + "*" + height},
            {"aid", "1250"},
            {"screen_width", width},
            {"os_api", "18"},
            {"ac", "WIFI"},
            {"os_version", "19.0.0"},
            {"device_platform", "iphone"},
            {"device_type", "iPhone14,5"},
            {"vid", ""},
            {"device_id", generateRandomString("1234567890", 11)},
            {"openudid", generateRandomString(alp, 40)},
            {"idfa", generateRandomString(alp, 40)}};
  this->Headers["User-Agent"] =
      "bcy 4.3.2 rv:4.3.2.6146 (iPad; iPhone OS 9.3.3; en_US) Cronet";
  this->Headers["Cookie"] = "";
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
http_response Core::GET(string URL, web::json::value Para,
                        map<string, string> Par) {
  if (Par.empty()) {
    Par = this->Params;
  }
  if (URL.find("http") == string::npos) {
    URL = APIBase + URL;
  }
  http_client_config cfg;
  if (this->proxy != "") {
    web::web_proxy proxy(this->proxy);
    cfg.set_proxy(proxy);
  }
#ifdef DEBUG
  cfg.set_validate_certificates(false);
#endif
  cfg.set_timeout(std::chrono::seconds(this->timeout));
  http_client client(U(URL), cfg);
  uri_builder builder;
  for (map<string, string>::iterator it = Par.begin(); it != Par.end(); it++) {
    string key = it->first;
    string val = it->second;
    builder.append_query(U(key), U(val));
  }
  string body = "";
  if (Para.is_null() == false) {
    for (auto iter = Para.as_object().cbegin(); iter != Para.as_object().cend();
         ++iter) {
      if (!body.empty()) {
        body += "&";
      }
      body += web::uri::encode_uri(iter->first) + "=" +
              web::uri::encode_uri(iter->second.as_string());
    }
  }

  for (decltype(retry) i = 0; i < retry + 1; i++) {
    try {
      http_request req(methods::GET);
      req.set_request_uri(builder.to_uri());
      req.headers() = this->Headers;
      req.set_body(body);
      pplx::task<web::http::http_response> task = client.request(req);
      return task.get();
    } catch (...) {
    }
  }
  http_request req(methods::GET);
  req.set_request_uri(builder.to_uri());
  req.headers() = this->Headers;
  req.set_body(body);
  pplx::task<web::http::http_response> task = client.request(req);
  task.wait();
  return task.get();
}
void Core::loginWithUIDAndSessionKey(std::string uid, std::string sKey) {
  this->UID = uid;
  this->sessionKey = sKey;
}
http_response Core::POST(string URL, web::json::value Para, bool Auth,
                         bool Encrypt, map<string, string> Par) {
  if (Par.empty()) {
    Par = this->Params;
  }
  if (URL.find("http") == string::npos) {
    URL = APIBase + URL;
  }
  if (Auth == true) {
    if (Para.has_field("session_key") == false && sessionKey != "") {
      Para["session_key"] = web::json::value(sessionKey);
    }
  }
  if (Encrypt == true) {
    Para = EncryptParam(Para);
  } else {
    Para = mixHEXParam(Para);
  }
  http_client_config cfg;
#ifdef DEBUG
  cfg.set_validate_certificates(false);
#endif
  cfg.set_timeout(std::chrono::seconds(this->timeout));
  if (this->proxy != "") {
    web::web_proxy proxy(this->proxy);
    cfg.set_proxy(proxy);
  }
  http_client client(U(URL), cfg);
  uri_builder builder;
  for (map<string, string>::iterator it = Par.begin(); it != Par.end(); it++) {
    string key = it->first;
    string val = it->second;
    builder.append_query(U(key), U(val));
  }
  string body = "";
  for (auto iter = Para.as_object().cbegin(); iter != Para.as_object().cend();
       ++iter) {
    if (!body.empty()) {
      body += "&";
    }
    body += web::uri::encode_uri(iter->first) + "=" +
            web::uri::encode_data_string(iter->second.as_string());
  }
  for (decltype(retry) i = 0; i < retry; i++) {
    try {
      http_request req(methods::POST);
      req.set_request_uri(builder.to_uri());
      req.headers() = this->Headers;
      req.set_body(body, "application/x-www-form-urlencoded");
      pplx::task<web::http::http_response> task = client.request(req);
      return task.get();
    } catch (...) {
    }
  }
  http_request req(methods::POST);
  req.set_request_uri(builder.to_uri());
  req.headers() = this->Headers;
  req.set_body(body, "application/x-www-form-urlencoded");
  pplx::task<web::http::http_response> task = client.request(req);
  task.wait();
  return task.get();
}
web::json::value Core::mixHEXParam(web::json::value Params) {
  if (Params.is_null()) {
    return web::json::value::null();
  }
  web::json::value j;
  for (auto iterInner = Params.as_object().cbegin();
       iterInner != Params.as_object().cend(); ++iterInner) {
    string K = iterInner->first;
    string V = iterInner->second.as_string();
    j[K] = web::json::value(bda_hexMixedString(V));
  }
  return j;
}
web::json::value Core::item_sharePost(string item_id) {
  web::json::value j;
  j["item_id"] = web::json::value(item_id);
  auto R = POST("api/item/sharePost", j, true, true);
  return R.extract_json().get();
}
web::json::value Core::ParamByCRC32URL(string FullURL) {
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
  for (decltype(results.size()) i = 3; i < results.size(); i++) {
    tmp << results[i] << "/";
  }
  string CRC32Candidate = tmp.str();
  CRC32Candidate = CRC32Candidate.substr(0, CRC32Candidate.length() - 1);
  string nonce = generateRandomString("1234567890", 16);
  CRC32Candidate = CRC32Candidate + "?r=" + nonce;
  CRC32 hash;
  unsigned int crc32_hash;
  CryptoPP::CRC32().CalculateDigest((CryptoPP::byte *)&crc32_hash,
                                    (CryptoPP::byte *)CRC32Candidate.c_str(),
                                    CRC32Candidate.size());
  web::json::value j;
  j["r"] = web::json::value(nonce);
  j["s"] = web::json::value(to_string(crc32_hash));
  return j;
}
web::json::value Core::videoInfo(string video_id) {
  // A few baseURLs, presumably CDN URLs.
  // The BaseURL is conjugated with the rest of the items. We should reverse the
  // algo to choose the correct CDN URL?
  string BaseURL = VideoCDNURLs[0];
  web::json::value CRC = ParamByCRC32URL(BaseURL + "/" + video_id);
  string nonce = CRC["r"].as_string();
  string crc = CRC["s"].as_string();
  map<string, string> P{{"r", nonce}, {"s", crc}};
  auto R = GET(BaseURL + "/" + video_id, json::value::null(), P);
  return R.extract_json().get();
}
web::json::value Core::timeline_friendfeed_hasmore(string since) {
  web::json::value j;
  j["since"] = web::json::value(since);
  auto R = POST("apiv2/timeline/friendfeed_hasmore", j, true, true);
  return R.extract_json().get();
}
web::json::value Core::timeline_stream_refresh() {
  web::json::value j;
  j["direction"] = web::json::value("refresh");
  auto R = POST("apiv2/timeline/stream", j, true, true);
  return R.extract_json().get();
}
web::json::value Core::qiniu_upload(web::json::value token, vector<char> &data,
                                    string extension) {
  string cloud_upToken = token["data"]["cloud_upToken"].as_string();
  string cloud_uploader = token["data"]["cloud_uploader"].as_string();
  string cloud_prefix = token["data"]["cloud_prefix"].as_string();
  posix_time::ptime pt = posix_time::second_clock::local_time();
  string dateStr = to_iso_string(pt).substr(0, 8);
  const string base36 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  /* log(2**64) / log(36) = 12.38 => max 13 char + '\0' */
  char buffer[14];
  unsigned int offset = sizeof(buffer);
  int value = boost::lexical_cast<int>(dateStr);
  buffer[--offset] = '\0';
  do {
    buffer[--offset] = base36[value % 36];
  } while (value /= 36);

  string base36Timestamp = string(&buffer[offset]);
  string fileName =
      generateRandomString(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXZY0123456789",
          0x20) +
      extension;
  string path = cloud_prefix + base36Timestamp + "/" + fileName;
  http_client_config cfg;
#ifdef DEBUG
  cfg.set_validate_certificates(false);
#endif
  cfg.set_timeout(std::chrono::seconds(this->timeout));
  if (this->proxy != "") {
    web::web_proxy proxy(this->proxy);
    cfg.set_proxy(proxy);
  }
  map<string, string> params;
  params["token"] = cloud_upToken;
  params["key"] = path;
  params["fileName"] = fileName;
  vector<unsigned char> body;
  for (map<string, string>::iterator it = params.begin(); it != params.end();
       it++) {
    string line = "--werghnvt54wef654rjuhgb56trtg34tweuyrgf\r\nContent-"
                  "Disposition: form-data; name=\"" +
                  it->first + "\"\r\n\r\n" + it->second + "\r\n";
    copy(line.begin(), line.end(), back_inserter(body));
  }
  string part1 = "--werghnvt54wef654rjuhgb56trtg34tweuyrgf\r\nContent-"
                 "Disposition: form-data; name=\"file\"; filename=\"" +
                 path + "\"\nContent-Type:image/jpeg\r\n\r\n";
  copy(part1.begin(), part1.end(), back_inserter(body));
  copy(data.begin(), data.end(), back_inserter(body));
  string part2 = "\r\n--werghnvt54wef654rjuhgb56trtg34tweuyrgf--\r\n";
  copy(part2.begin(), part2.end(), back_inserter(body));

  http_request req;
  http_client client(U(cloud_uploader));
  req.set_method(methods::POST);
  req.headers().add("Content-Length", to_string(body.size()));
  req.headers().add(
      "Content-Type",
      "multipart/form-data; boundary=werghnvt54wef654rjuhgb56trtg34tweuyrgf");
  req.set_body(body);
  return client.request(req).get().extract_json().get();
} // namespace BCY
web::json::value Core::item_postUploadToken() {
  web::json::value j;
  auto R = POST("api/item/postUpLoadToken", j, true, true);
  return R.extract_json().get();
}
web::json::value Core::item_postUploadToken(string GID) {
  web::json::value j;
  j["gid"] = web::json::value(GID);
  auto R = POST("api/item/postUpLoadToken", j, true, true);
  return R.extract_json().get();
}
web::json::value Core::timeline_stream_loadmore(string feed_type,
                                                int first_enter,
                                                int refresh_num) {
  web::json::value j;
  j["direction"] = web::json::value("loadmore");
  j["feed_type"] = web::json::value(feed_type);
  j["first_enter"] = web::json::value(first_enter);
  j["refresh_num"] = web::json::value(refresh_num);
  auto R = POST("apiv2/timeline/stream", j, true, true);
  return R.extract_json().get();
}
web::json::value Core::EncryptParam(web::json::value Params) {
  string serialized = Params.serialize();
  web::json::value j;
  string foo;
  Base64::Encode(EncryptData(serialized), &foo);
  j["data"] = web::json::value(foo);
  return j;
}
string Core::bda_hexMixedString(string input) {
  stringstream os;
  static const char key = 5;
  os << setfill('0') << setw(2);
  stringstream os2;
  for (decltype(input.length()) i = 0; i < input.length(); i++) {
    char Val = input[i] ^ key;
    os2 << hex << Val;
  }
  os << os2.str();
  return BCY::bcy_string_to_hex(os.str());
}
web::json::value Core::space_me() {
  web::json::value j;
  auto R = POST("api/space/me", j, true, true).extract_json().get();
  return R;
}
web::json::value Core::loginWithEmailAndPassword(string email,
                                                 string password) {
  web::json::value j;
  j["email"] = web::json::value(email);
  j["password"] = web::json::value(password);
  web::json::value LoginResponse =
      POST("passport/email/login/", j, false, false).extract_json().get();
  if (LoginResponse["message"].as_string() == "error") {
    string msg = LoginResponse["data"]["description"].as_string();

    return LoginResponse;
  }
  sessionKey = LoginResponse["data"]["session_key"].as_string();
  j = json::value::null();
  auto R = POST("api/token/doLogin", j, true, true);

  Headers["Cookie"] = R.headers()["Set-Cookie"];

  LoginResponse = R.extract_json().get();
  UID = LoginResponse["data"]["uid"].as_string();
  return LoginResponse;
}
web::json::value Core::user_detail(string UID) {
  web::json::value j;
  j["uid"] = web::json::value(UID);
  auto R = POST("api/token/doLogin", j, true, true);
  return R.extract_json().get();
}
web::json::value Core::image_postCover(string item_id, string type) {
  web::json::value j;
  j["id"] = web::json::value(item_id);
  j["type"] = web::json::value(type);
  auto R = POST("api/image/postCover/", j, true, true);
  return R.extract_json().get();
}
bool Core::user_follow(string uid, bool isFollow) {
  web::json::value j;
  j["uid"] = web::json::value(uid);
  if (isFollow) {
    j["type"] = web::json::value("dofollow");
  } else {
    j["type"] = web::json::value("unfollow");
  }
  web::json::value R =
      POST("api/user/follow", j, true, true).extract_json().get();
  return R["status"].as_integer() == 1;
}
web::json::value Core::item_detail(string item_id, bool autoFollow) {
  web::json::value j;
  j["item_id"] = web::json::value(item_id);
  auto R = POST("api/item/detail/", j, true, true);
  web::json::value r = R.extract_json().get();
  if (r["status"] == 4010 && autoFollow) {
    // Need to Follow
    string UID = r["data"]["profile"]["uid"].as_string();
    user_follow(UID, true);
    R = POST("api/item/detail/", j, true, true);
    r = R.extract_json().get();
    user_follow(UID, false);
  }
  return r;
}
bool Core::item_doPostLike(string item_id) {
  web::json::value j;
  j["item_id"] = web::json::value(item_id);
  auto R = POST("api/item/doPostLike", j, true, true);
  web::json::value r = R.extract_json().get();
  return r["status"] == 1;
}
bool Core::item_cancelPostLike(string item_id) {
  web::json::value j;
  j["item_id"] = web::json::value(item_id);
  auto R = POST("api/item/cancelPostLike", j, true, true);
  web::json::value r = R.extract_json().get();
  return r["status"] == 1;
}
web::json::value Core::item_postUpLoadParam() {
  web::json::value j;
  auto R = POST("api/item/postUpLoadParam", j, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
web::json::value Core::item_doNewPost(NewPostType type, web::json::value args) {
  string URL = "/api/item/doNew";
  switch (type) {
  case NewPostType::GroupAnswer: {
    URL = URL + "GroupAnswer";
    break;
  }
  case NewPostType::ArticlePost: {
    URL = URL + "ArticlePost";
    break;
  }
  case NewPostType::NotePost: {
    URL = URL + "NotePost";
    break;
  }
  default: {
    throw invalid_argument("Invalid NewPost Type!");
  }
  }
  web::json::value token = item_postUpLoadParam()["data"]["post_token"];
  args["post_token"] = token;
  auto R = POST(URL, args, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
web::json::value
Core::prepareNoteUploadArg(vector<string> &tags,
                           vector<struct UploadImageInfo> &Infos,
                           string content, bool allowSave, bool addWatermark,
                           bool modify, bool trans, VisibilityType view) {
  web::json::value j;
  string vType = "";
  switch (view) {
  case (VisibilityType::All): {
    vType = "all";
    break;
  }
  case (VisibilityType::Login): {
    vType = "login";
    break;
  }
  case (VisibilityType::Fans): {
    vType = "fans";
    break;
  }
  default: {
    throw invalid_argument("Unknown VisibilityType");
    break;
  }
  }
  vector<web::json::value> Tags;
  vector<web::json::value> URLs;
  for (string tag : tags) {
    Tags.push_back(web::json::value(tag));
  }
  j["tag_names"] = web::json::value::array(Tags);
  for (struct UploadImageInfo inf : Infos) {
    web::json::value ele;
    ele["h"] = web::json::value(inf.Height);
    ele["w"] = web::json::value(inf.Width);
    ele["path"] = web::json::value(inf.URL);
    ele["ratio"] = web::json::value(inf.Ratio);
    URLs.push_back(ele);
  }
  j["multi"] = web::json::value::array(URLs);
  j["content"] = web::json::value(content);
  web::json::value x;
  web::json::value y;
  y["download"] = web::json::value(allowSave);
  y["water_mark"] = web::json::value(addWatermark);
  x["save"] = y;
  web::json::value z;
  z["no_modify"] = web::json::value(!modify);
  z["no_trans"] = web::json::value(!trans);
  x["transmit"] = z;
  x["view"] = web::json::value(vType);
  j["authority"] = x;
  return j;
}
web::json::value Core::deletePost(string item_id) {
  web::json::value j;
  j["item_id"] = web::json::value(item_id);
  auto R = POST("api/item/deletePost", j, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
web::json::value Core::tag_status(string TagName) {
  web::json::value j;
  j["name"] = web::json::value(TagName);
  auto R = POST("api/tag/status", j, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
web::json::value Core::core_status(string WorkID) {
  web::json::value j;
  j["wid"] = web::json::value(WorkID);
  auto R = POST("api/core/status", j, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
web::json::value Core::user_userTagList() {
  web::json::value j;
  auto R = POST("api/user/userTagList", j, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
web::json::value Core::user_getUserTag(string uid) {
  web::json::value j;
  j["uid"] = web::json::value(uid);
  auto R = POST("api/user/getUserTag", j, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
web::json::value Core::circle_filterlist(string circle_id,
                                         CircleType circle_type,
                                         string circle_name) {
  web::json::value j;
  j["circle_id"] = web::json::value(circle_id);
  switch (circle_type) {
  case CircleType::Tag: {
    j["circle_type"] = web::json::value("tag");
    break;
  }
  case CircleType::Work: {
    j["circle_type"] = web::json::value("work");
    break;
  }
  default: {
    throw invalid_argument("Invalid Circle Type!");
  }
  }
  j["circle_name"] = web::json::value(circle_name);
  auto R = POST("apiv2/circle/filterlist/", j, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
vector<web::json::value>
Core::search_item_bytag(list<string> TagNames, PType ptype,
                        BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  int p = 0;
  web::json::value j;
  if (ptype != PType::Undef) {
    j["ptype"] = web::json::value(static_cast<uint32_t>(ptype));
  }
  vector<web::json::value> tmp;
  for (string str : TagNames) {
    tmp.push_back(web::json::value(str));
  }
  j["tag_list"] = web::json::value::array(tmp);
  while (true) {
    j["p"] = p;
    p++;
    auto R = POST("apiv2/search/item/bytag/", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"]["ItemList"];
    if (data.size() == 0) {
      return ret;
    }
    for (web::json::value &ele : data.as_array()) {
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
web::json::value Core::group_detail(string GID) {
  web::json::value j;
  j["gid"] = web::json::value(GID);
  auto R = POST("api/group/detail/", j, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
vector<web::json::value>
Core::circle_itemhotworks(string circle_id, BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  int since = 0;
  web::json::value j;
  j["grid_type"] = web::json::value("timeline");
  j["id"] = web::json::value(circle_id);
  while (true) {
    if (since == 0) {
      j["since"] = web::json::value(since);
    } else {
      j["since"] = web::json::value("rec:" + to_string(since));
    }
    since++;
    auto R = POST("apiv2/circle/itemhotworks/", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"]["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (web::json::value &ele : data.as_array()) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
}
vector<web::json::value>
Core::circle_itemrecentworks(uint64_t circle_id, string name,
                             BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  string since = "0";
  web::json::value j;
  j["grid_type"] = web::json::value("timeline");
  j["id"] = web::json::value(circle_id);
  j["name"] = web::json::value(name);
  // j["filter"] = web::json::value("all");
  while (true) {
    j["since"] = web::json::value(since);
    auto R = POST("apiv2/circle/item/recent/works", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (web::json::value &ele : data.as_array()) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
    since = data[data.as_array().size() - 1]["since"].as_string();
  }
}
vector<web::json::value>
Core::circle_itemrecenttags(string name, string filter,
                            BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  string since = "0";
  web::json::value j;
  j["grid_type"] = web::json::value("timeline");
  j["id"] = tag_status(name)["data"]["tag_id"];
  j["name"] = web::json::value(name);
  j["filter"] = web::json::value("all");
  while (true) {
    j["since"] = web::json::value(since);
    auto R = POST("apiv2/circle/item/recent/tags", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (web::json::value &ele : data.as_array()) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
    since = data[data.as_array().size() - 1]["since"].as_string();
  }
}
vector<web::json::value>
Core::circle_itemhottags(string name, BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  uint64_t since = 0;
  web::json::value j;
  j["grid_type"] = web::json::value("timeline");
  j["id"] = tag_status(name)["data"]["tag_id"];
  j["name"] = web::json::value(name);
  // j["filter"] = web::json::value("all");
  while (true) {
    if (since == 0) {
      j["since"] = web::json::value(since);
    } else {
      j["since"] = web::json::value("rec:" + to_string(since));
    }
    since++;
    auto R = POST("apiv2/circle/itemhottags", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"]["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (web::json::value &ele : data.as_array()) {
      if (callback) {
        if (!callback(ele)) {
          return ret;
        }
      }
      ret.push_back(ele);
    }
  }
}
web::json::value Core::event_detail(string event_id) {
  web::json::value j;
  j["event_id"] = web::json::value(event_id);
  auto R = POST("api/event/detail", j, true, true);
  web::json::value r = R.extract_json().get();
  return r;
}
vector<web::json::value> Core::item_getReply(string item_id,
                                             BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  int p = 1;
  web::json::value j;
  j["item_id"] = web::json::value(item_id);
  while (true) {
    j["p"] = p;
    p++;
    auto R = POST("api/item/getReply", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (web::json::value &ele : data.as_array()) {
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
vector<web::json::value>
Core::circle_itemRecentWorks(string WorkID, BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  string since = "";
  web::json::value j;
  j["grid_type"] = web::json::value("timeline");
  j["id"] = web::json::value(WorkID);
  while (true) {
    j["since"] = web::json::value(since);
    auto R = POST("api/circle/itemRecentWorks", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"].as_string();
    for (web::json::value &ele : data.as_array()) {
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
vector<web::json::value>
Core::timeline_getUserPostTimeLine(string UID,
                                   BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  string since = "0";
  web::json::value j;
  j["grid_type"] = web::json::value("timeline");
  j["uid"] = web::json::value(UID);
  while (true) {
    j["since"] = web::json::value(since);
    auto R = POST("api/timeline/getUserPostTimeLine/", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"].as_string();
    for (web::json::value &ele : data.as_array()) {
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
vector<web::json::value>
Core::space_getUserLikeTimeLine(string UID, BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  string since = "0";
  web::json::value j;
  j["grid_type"] = web::json::value("grid");
  j["uid"] = web::json::value(UID);
  while (true) {
    j["since"] = web::json::value(since);
    auto R = POST("api/space/getUserLikeTimeLine/", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"].as_string();
    for (web::json::value &ele : data.as_array()) {
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
vector<web::json::value>
Core::event_listPosts(string event_id, Order ord,
                      BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  int p = 1;
  web::json::value j;
  switch (ord) {
  case Order::Hot: {
    j["order"] = web::json::value("hot");
    break;
  }
  case Order::Index: {
    j["order"] = web::json::value("index");
    break;
  }
  default: {
    throw invalid_argument("Invalid Order Type!");
  }
  }
  j["event_id"] = web::json::value(event_id);
  while (true) {
    j["p"] = p;
    p++;
    auto R = POST("api/event/listPosts", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (web::json::value &ele : data.as_array()) {
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
vector<web::json::value> Core::search(string keyword, SearchType type,
                                      BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  int p = 1;
  string URL = "api/search/search";
  switch (type) {
  case SearchType::Content: {
    URL = URL + "Content";
    break;
  }
  case SearchType::Works: {
    URL = URL + "Works";
    break;
  }
  case SearchType::Tags: {
    URL = URL + "Tags";
    break;
  }
  case SearchType::User: {
    URL = URL + "User";
    break;
  }
  default: {
    throw invalid_argument("Invalid Search Type!");
  }
  }
  web::json::value j;
  j["query"] = web::json::value(keyword);
  while (true) {
    j["p"] = web::json::value(p);
    p++;
    auto R = POST(URL, j, true, true);

    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"]["results"];
    if (data.size() == 0) {
      return ret;
    }
    for (web::json::value &ele : data.as_array()) {
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
vector<web::json::value>
Core::group_listPosts(string GID, BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  int p = 1;
  web::json::value j;
  j["gid"] = web::json::value(GID);
  j["type"] = web::json::value("ding");
  j["limit"] = web::json::value(50);
  while (true) {
    j["p"] = p;
    p++;
    auto R = POST("api/group/listPosts", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    for (web::json::value &ele : data.as_array()) {
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
vector<web::json::value>
Core::timeline_friendfeed(BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  std::chrono::duration<float> ms =
      std::chrono::duration_cast<std::chrono::duration<float>>(
          std::chrono::system_clock::now().time_since_epoch());
  string since = to_string(ms.count());
  web::json::value j;
  j["grid_type"] = web::json::value("timeline");
  j["direction"] = web::json::value("refresh");
  bool firstTime = true;
  while (true) {
    j["since"] = web::json::value(since);
    auto R = POST("apiv2/timeline/friendfeed", j, true, true);
    j["direction"] = web::json::value("loadmore");
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0 && firstTime == false) {
      return ret;
    }
    if (firstTime == true) {
      firstTime = false;
      continue;
    }
    since = data[data.size() - 1]["since"].as_string();
    for (web::json::value &ele : data.as_array()) {
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
vector<web::json::value>
Core::item_favor_itemlist(BCYListIteratorCallback callback) {
  vector<web::json::value> ret;
  string since = "0";
  web::json::value j;
  j["grid_type"] = web::json::value("grid");
  while (true) {
    j["since"] = web::json::value(since);
    auto R = POST("apiv2/item/favor/itemlist", j, true, true);
    web::json::value foo = R.extract_json().get();
    web::json::value data = foo["data"];
    if (data.size() == 0) {
      return ret;
    }
    since = data[data.size() - 1]["since"].as_string();
    for (web::json::value &ele : data.as_array()) {
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
