
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
namespace BCY {
//!  Core class used for interacting with BCY API
class Core {
public:
    /** @enum SearchType
     *  @brief Type of the Search to perform
     */
    enum class SearchType {
        Content = 0, /**< Posts */
        Works,       /**< Works */
        Tags,        /**< Tags */
        User,        /**< Users */
    };
    /** @enum CircleType
     *  @brief Type of a Circle
     */
    enum class CircleType {
        Tag = 0, /**< Tag */
        Work,    /**< Work */
    };
    /** @enum Order
     *  @brief Order
     */
    enum class Order {
        Hot = 0, /**< Hottest */
        Index,   /**< Timeline */
    };
    /** @enum PType
     *  @brief First class filter types. Other tags are treated as normal tags
     */
    enum class PType {
        Image = 1,   /**< Image */
        Text  = 2,   /**< Text */
        Video = 3,   /**< Video */
        Undef = 999, /**< Undefined.*/
    };
    enum class NewPostType {
        GroupAnswer = 0,
        ArticlePost,
        NotePost,
    };
    enum class PublishType {
        Note    = 0,
        Article = 1,
        Gask    = 3,
        Ganswer = 4,
        other   = 999,
    };
    enum class VisibilityType {
        All   = 0,
        Login = 1,
        Fans  = 2,
    };
    struct UploadImageInfo {
        std::string URL;
        float Height;
        float Width;
        float Ratio;
    };
    /**
     * \typedef BCYListIteratorCallback
     * Callback function prototype called by list iterators.
     * Return false to leave early. Otherwise the function returns until the list is empty
     * @param item An item in the list
     */
    typedef std::function<bool(web::json::value& item)> BCYListIteratorCallback;
    Core();
    std::string UID      = "";
    std::string proxy    = "";
    unsigned int retry   = 3;
    unsigned int timeout = 10;
    web::json::value item_sharePost(std::string item_id);
    web::http::http_response POST(std::string URL, web::json::value Payload = web::json::value(), bool Auth = true,
                                  bool EncryptParam                         = true,
                                  std::map<std::string, std::string> Params = std::map<std::string, std::string>());
    web::http::http_response GET(std::string URL, web::json::value Payload = web::json::value(),
                                 std::map<std::string, std::string> Params = std::map<std::string, std::string>());
    bool item_doPostLike(std::string item_id);
    bool item_cancelPostLike(std::string item_id);
    bool user_follow(std::string uid, bool isFollow);
    web::json::value user_detail(std::string UID);
    web::json::value image_postCover(std::string item_id);
    web::json::value space_me();
    web::json::value prepareNoteUploadArg(std::vector<std::string>& tags, std::vector<struct UploadImageInfo>& Infos,
                                          std::string content = "", bool allowSave = false, bool addWatermark = true,
                                          bool modify = false, bool trans = false,
                                          VisibilityType view = VisibilityType::All);
    /**
     * Login with email and password
     * \param email E-Mail address
     * \param password Password
     * \return On failure, has ``[data][description]`` which contains failure reason. On success can be ignored
     */
    web::json::value loginWithEmailAndPassword(std::string email, std::string password);
    /**
     * Login with UID and sessionKey
     * \param uid User's UID.
     * \param sessionKey Session Key. Can be dumped from a jailbroken iOS device or a rooted Android device.
     */
    void loginWithUIDAndSessionKey(std::string uid, std::string sessionKey);
    web::json::value item_detail(std::string item_id, bool autoFollow = true);
    web::json::value tag_status(std::string TagName);
    web::json::value user_getUserTag(std::string uid);
    web::json::value deletePost(std::string item_id);
    web::json::value event_detail(std::string event_id); // https://bcy.net/tags/event for a full event list
    /**
     * Returns a list of user-assignable tags and their ut_id
     */
    web::json::value user_userTagList(); // A list of all available user tags and their ut_id

    web::json::value circle_filterlist(std::string circle_id, CircleType circle_type, std::string circle_name);
    /**
     * List Hot items of a tag
     * \return ``{"status":1,"message":"","debug_info":"","data":{"id":"123456","name":"name","relative_wid":-1,"type":"tag","nickname":"","intro":"圈子欢迎你","cover":"https://p9-bcy.byteimg.com","admin_users":[],"follow_status":true,"follow_count":17,"affiches":[],"announces":[],"members":[{"uid":1,"uname":"X","avatar":"https://"},{"uid":1,"uname":"X","avatar":"https://"},{"uid":1,"uname":"X","avatar":"https://"}]}}``
     */
    std::vector<web::json::value> circle_itemhottags(std::string name,
                                                     BCYListIteratorCallback callback = BCYListIteratorCallback());
    web::json::value group_detail(std::string GID);
    web::json::value timeline_stream_refresh();
    web::json::value item_postUploadToken();
    web::json::value item_postUpLoadParam();
    web::json::value qiniu_upload(web::json::value token, std::vector<char>& data, std::string extension = "jpg");
    web::json::value item_doNewPost(NewPostType type, web::json::value args);
    web::json::value item_postUploadToken(std::string GID);
    web::json::value timeline_stream_loadmore(std::string feed_type = "", int first_enter = 1, int refresh_num = 25);
    web::json::value core_status(std::string WorkID);
    web::json::value videoInfo(std::string video_id);
    web::json::value timeline_friendfeed_hasmore(std::string since);
    std::vector<web::json::value> event_listPosts(std::string event_id, Order ord = Order::Hot,
                                                  BCYListIteratorCallback callback = BCYListIteratorCallback());
  /**
   * Status of a circle by its name
   * \return ``{"status":1,"message":"","debug_info":"","data":{"id":"5320","name":"name","relative_wid":-1,"type":"tag","nickname":"","intro":"圈子欢迎你","cover":"https://p9-bcy.byteimg.com","admin_users":[],"follow_status":true,"follow_count":17,"affiches":[],"announces":[],"members":[{"uid":1,"uname":"X","avatar":"https://"},{"uid":1,"uname":"X","avatar":"https://"},{"uid":1,"uname":"X","avatar":"https://"}]}}``
   */
    web::json::value circle_status(std::string name);
    std::vector<web::json::value> circle_itemrecenttags(std::string TagName, std::string Filter,
                                                        BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> item_getReply(std::string item_id,
                                                BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> search_item_bytag(std::list<std::string> TagNames, PType ptype = PType::Undef,
                                                    BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> circle_itemRecentWorks(std::string WorkID,
                                                         BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> timeline_getUserPostTimeLine(
        std::string UID, BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> space_getUserLikeTimeLine(
        std::string UID, BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> circle_itemrecentworks(uint64_t circle_id, std::string name,
                                                         BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> circle_itemrecenttags(std::string name,
                                                        BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> search(std::string keyword, SearchType type,
                                         BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> group_listPosts(std::string GID,
                                                  BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> timeline_friendfeed(BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> circle_itemhotworks(std::string circle_id,
                                                      BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> item_favor_itemlist(BCYListIteratorCallback callback = BCYListIteratorCallback());
    std::vector<web::json::value> user_followlist(std::string UID,std::string type,BCYListIteratorCallback callback= BCYListIteratorCallback());
private:
    std::string bda_hexMixedString(std::string input);
    std::map<std::string, std::string> Params;
    web::http::http_headers Headers;
    std::string sessionKey = "";
    std::string EncryptData(std::string Input);
    web::json::value EncryptParam(web::json::value Params);
    web::json::value mixHEXParam(web::json::value Params);
    web::json::value ParamByCRC32URL(std::string FullURL);
};
} // namespace BCY
#endif
