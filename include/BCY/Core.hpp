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
typedef std::function<bool(nlohmann::json &)>
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
  nlohmann::json EncryptParam(nlohmann::json Params);
  nlohmann::json mixHEXParam(nlohmann::json Params);
  cpr::Response POST(std::string URL, nlohmann::json Payload=nlohmann::json(), bool Auth=true,
                     bool EncryptParam=true,cpr::Parameters Para=cpr::Parameters());
  cpr::Response GET(std::string URL,nlohmann::json Payload=nlohmann::json(),cpr::Parameters Para=cpr::Parameters());
  nlohmann::json user_detail(std::string UID);
  nlohmann::json image_postCover(std::string item_id, std::string type);
  bool user_follow(std::string uid, bool isFollow);
  nlohmann::json space_me();
  nlohmann::json loginWithEmailAndPassword(std::string email, std::string password);
  nlohmann::json item_detail(std::string item_id, bool autoFollow = true);
  bool item_doPostLike(std::string item_id);
  bool item_cancelPostLike(std::string item_id);
  nlohmann::json tag_status(std::string TagName);
  nlohmann::json circle_filterlist(std::string circle_id, CircleType circle_type,
                         std::string circle_name);
  nlohmann::json circle_itemhottags(std::string item_id);
  nlohmann::json group_detail(std::string GID);
  nlohmann::json core_status(std::string WorkID);
  nlohmann::json ParamByCRC32URL(std::string FullURL);
  nlohmann::json videoInfo(std::string video_id);
  std::vector<nlohmann::json> circle_itemrecenttags(
      std::string TagName, std::string Filter,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json>
  item_getReply(std::string item_id,
                BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json> search_item_bytag(
      std::list<std::string> TagNames, PType ptype = PType::Undef,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json> circle_itemRecentWorks(
      std::string WorkID,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json> timeline_getUserPostTimeLine(
      std::string UID,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json> space_getUserLikeTimeLine(
      std::string UID,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json>
  search(std::string keyword, SearchType type,
         BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json>
  group_listPosts(std::string GID,
                  BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json> timeline_friendfeed(
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json> circle_itemhotworks(
      std::string circle_id,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<nlohmann::json> item_favor_itemlist(
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
