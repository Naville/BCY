#ifndef BCY_DOWNLOADUTILS_HPP
#define BCY_DOWNLOADUTILS_HPP
#include <BCY/Core.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>
#include <cryptopp/md5.h>
#include <set>
#include <tuple>
#include <optional>
namespace BCY {
    class DownloadFilter;
class DownloadUtils {

public:
    typedef std::tuple<std::string/*UID*/,std::string/*item_id*/,std::string/*Title*/,std::vector<std::string>/*Tags*/,std::string/*ctime*/,
    std::string/*Description*/,web::json::array/*multi*/,std::string/*videoID*/> Info;
  Core core;
  DownloadFilter *filter = nullptr;
  bool useCachedInfo = true;
  bool downloadVideo = true;
  bool enableFilter = true;
  std::string secret = "";
  std::string RPCServer = "";
  ~DownloadUtils();
  DownloadUtils(DownloadUtils &) = delete;
  DownloadUtils() = delete;
  DownloadUtils(std::string PathBase, int queryThreadCount = -1,
                int downloadThreadCount = -1,
                std::string DBPath = ""); //-1 to use hardware thread count
    Info  canonicalizeRawServerDetail(web::json::value);
  void downloadFromAbstractInfo(web::json::value& Inf, bool runFilter);
  void downloadFromAbstractInfo(web::json::value& Inf);
  void downloadFromInfo(DownloadUtils::Info Inf,bool runFilter = true);
  // Will try to extract item_id from info first, if not it loads from the
  // argument,if still invalid an error is displayed and download cancelled
  std::string loadTitle(std::string title,std::string item_id);
    void saveInfo(Info);
  std::optional<Info> loadInfo(std::string item_id);
  std::string loadOrSaveGroupName(std::string name, std::string GID);
  web::json::value loadEventInfo(std::string event_id);
  void insertEventInfo(web::json::value Inf);
  void cleanup();
  void join();
  web::json::value saveOrLoadUser(std::string uid,std::string uname,std::string intro,std::string avatar,bool isValueUser);
  boost::filesystem::path getUserPath(std::string UID);
  boost::filesystem::path getItemPath(std::string UID,std::string item_id);
  void cleanUID(std::string UID);
  void cleanItem(std::string ItemID);
  void cleanTag(std::string Tag);
  void verify(std::string condition = "", std::vector<std::string> args = {},
              bool reverse = false);
  void verifyUID(std::string UID, bool reverse = false);
  void verifyTag(std::string Tag, bool reverse = false);
  void addTypeFilter(std::string filter);
  void downloadGroupID(std::string gid);
  void downloadWorkID(std::string item);
  void downloadUserLiked(std::string uid);
  void downloadLiked();
  void downloadTag(std::string TagName);
  void downloadEvent(std::string event_id);
  void downloadUser(std::string uid);
  void downloadSearchKeyword(std::string KW);
  void downloadTimeline();
  void downloadItemID(std::string item_id);
  void unlikeCached();
  void downloadHotTags(std::string TagName,unsigned int cnt=20000);
  void downloadHotWorks(std::string id,unsigned int cnt=20000);

private:
  std::string md5(std::string &str);
  boost::asio::thread_pool *queryThread = nullptr;
  boost::asio::thread_pool *downloadThread = nullptr;
  std::mutex dbLock;
  std::string saveRoot;
  std::string DBPath;
  std::set<std::string> typeFilters;
  std::function<bool(web::json::value)> downloadCallback =
      [&](web::json::value j) {
        this->downloadFromAbstractInfo(j);
        return true;
      };
  bool stop = false;
};
} // namespace BCY
#endif
