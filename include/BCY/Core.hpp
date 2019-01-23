#ifndef BCY_CORE_HPP
#define BCY_CORE_HPP
#include <cpprest/http_msg.h>
#include <cpprest/json.h>
#include <cryptopp/aes.h>
#include <cryptopp/crc.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/modes.h>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
typedef std::function<bool(web::json::value &)>
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
  std::string UID = "";
  std::string proxy = "";
  unsigned int retry = 3;
  unsigned int timeout = 10;
  std::string EncryptData(std::string Input);
  web::json::value EncryptParam(web::json::value Params);
  web::json::value mixHEXParam(web::json::value Params);
  web::http::http_response POST(std::string URL,
                                web::json::value Payload = web::json::value(),
                                bool Auth = true, bool EncryptParam = true,
                                std::map<std::string, std::string> Params =
                                    std::map<std::string, std::string>());
  web::http::http_response GET(std::string URL,
                               web::json::value Payload = web::json::value(),
                               std::map<std::string, std::string> Params =
                                   std::map<std::string, std::string>());
  web::json::value user_detail(std::string UID);
  web::json::value image_postCover(std::string item_id, std::string type);
  bool user_follow(std::string uid, bool isFollow);
  web::json::value space_me();
  web::json::value loginWithEmailAndPassword(std::string email,
                                             std::string password);
  web::json::value item_detail(std::string item_id, bool autoFollow = true);
  bool item_doPostLike(std::string item_id);
  bool item_cancelPostLike(std::string item_id);
  web::json::value tag_status(std::string TagName);
  web::json::value circle_filterlist(std::string circle_id,
                                     CircleType circle_type,
                                     std::string circle_name);
  web::json::value circle_itemhottags(std::string item_id);
  web::json::value group_detail(std::string GID);
  web::json::value timeline_stream_refresh();
  web::json::value timeline_stream_loadmore(std::string feed_type="",int first_enter=1,int refresh_num=25);
  web::json::value core_status(std::string WorkID);
  web::json::value ParamByCRC32URL(std::string FullURL);
  web::json::value videoInfo(std::string video_id);
    web::json::value timeline_friendfeed_hasmore(std::string since);
  std::vector<web::json::value> circle_itemrecenttags(
      std::string TagName, std::string Filter,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value>
  item_getReply(std::string item_id,
                BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value> search_item_bytag(
      std::list<std::string> TagNames, PType ptype = PType::Undef,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value> circle_itemRecentWorks(
      std::string WorkID,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value> timeline_getUserPostTimeLine(
      std::string UID,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value> space_getUserLikeTimeLine(
      std::string UID,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value>
  search(std::string keyword, SearchType type,
         BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value>
  group_listPosts(std::string GID,
                  BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value> timeline_friendfeed(
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value> circle_itemhotworks(
      std::string circle_id,
      BCYListIteratorCallback callback = BCYListIteratorCallback());
  std::vector<web::json::value> item_favor_itemlist(
      BCYListIteratorCallback callback = BCYListIteratorCallback());

private:
  std::string bda_hexMixedString(std::string input);
  std::map<std::string, std::string> Params;
  web::http::http_headers Headers;
  std::string sessionKey = "";
};
} // namespace BCY
#endif
