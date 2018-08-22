#ifndef BCY_CORE_HPP
#define BCY_CORE_HPP
#include "json.hpp"
#include <cpr/cpr.h>
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/crc.h>
#include <cryptopp/hex.h>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
using json = nlohmann::json;
typedef std::function<bool(json &)>
    BCYListIteratorCallback; // Return false so the iterators leave early

namespace BCY {
enum class SearchType {
  Content = 0,
  Works,
  Tags,
  User,
};
enum class CircleType {
  Tag = 0, //"tag"
  Work,    //"work"
};
enum class PType { // First class Filter Types,Everything else is treated as
                   // normal tag
  Image = 1,
  Text = 2,
  Video = 3,
  Undef = 999,
};

class Core {
public:
  Core();
  Core(Core &) = delete;
  cpr::Proxies proxy;
  std::string UID = "";
  int retry = 3;
    int timeout = 10000;
  std::function<void(cpr::Error, std::string)> errorHandler;
  std::string EncryptData(std::string Input);
  json EncryptParam(json Params);
  json mixHEXParam(json Params);
  cpr::Response POST(std::string URL, json Payload=json(), bool Auth=true,
                     bool EncryptParam=true,cpr::Parameters Para=cpr::Parameters());
  cpr::Response GET(std::string URL,json Payload=json(),cpr::Parameters Para=cpr::Parameters());
  json user_detail(std::string UID);
  json image_postCover(std::string item_id, std::string type);
  bool user_follow(std::string uid, bool isFollow);
  json space_me();
  json loginWithEmailAndPassword(std::string email, std::string password);
  json item_detail(std::string item_id, bool autoFollow = true);
  bool item_doPostLike(std::string item_id);
  bool item_cancelPostLike(std::string item_id);
  json tag_status(std::string TagName);
  json circle_filterlist(std::string circle_id, CircleType circle_type,
                         std::string circle_name);
  json circle_itemhottags(std::string item_id);
  json group_detail(std::string GID);
  json core_status(std::string WorkID);
  json ParamByCRC32URL(std::string FullURL);
  json videoInfo(std::string video_id);
  std::vector<json> circle_itemrecenttags(
      std::string TagName, std::string Filter,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json>
  item_getReply(std::string item_id,
                BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json> search_item_bytag(
      std::list<std::string> TagNames, PType ptype = PType::Undef,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json> circle_itemRecentWorks(
      std::string WorkID,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json> timeline_getUserPostTimeLine(
      std::string UID,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json> space_getUserLikeTimeLine(
      std::string UID,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json>
  search(std::string keyword, SearchType type,
         BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json>
  group_listPosts(std::string GID,
                  BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json> timeline_friendfeed(
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json> circle_itemhotworks(
      std::string circle_id,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<json> item_favor_itemlist(
      BCYListIteratorCallback callback = BCYListIteratorCallback());
private:
  std::mutex sessionLock;
  std::string bda_hexMixedString(std::string input);
  cpr::Session Sess;
  cpr::Parameters Params;
  std::string sessionKey = "";
};
} // namespace BCY
#endif
