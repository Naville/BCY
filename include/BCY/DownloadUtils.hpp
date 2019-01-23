#ifndef BCY_DOWNLOADUTILS_HPP
#define BCY_DOWNLOADUTILS_HPP
#include "BCY/Core.hpp"
#include "BCY/DownloadFilter.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>
#include <cryptopp/md5.h>
#include <set>
namespace BCY {
class DownloadUtils {
public:
  Core core;
  DownloadFilter *filter = nullptr;
  bool useCachedInfo = true;    // Would be really slow for large databases
  bool allowCompressed = false; // if enabled, downloader will try to use the
  // official API to download compressed
  // (~20%quality lost )images instead of
  // stripping URL to download the original one
  std::string secret = "";
  std::string RPCServer = "";
  ~DownloadUtils();
  DownloadUtils(DownloadUtils &) = delete;
  DownloadUtils() = delete;
  DownloadUtils(std::string PathBase, int queryThreadCount = -1,
                int downloadThreadCount = -1,
                std::string DBPath = ""); //-1 to use hardware thread count
  void downloadFromAbstractInfo(web::json::value Inf);
  void downloadFromInfo(web::json::value Inf, bool save = true,
                        std::string item_id_arg = "");
  // Will try to extract item_id from info first, if not it loads from the
  // argument,if still invalid an error is displayed and download cancelled
  std::string loadTitle(std::string title, web::json::value Inf);
  void saveInfo(std::string title, web::json::value Inf);
  web::json::value loadInfo(std::string item_id);
  std::string loadOrSaveGroupName(std::string name, std::string GID);
  void insertRecordForCompressedImage(std::string item_id);
  void cleanup();
  void join();
  void cleanUID(std::string UID);
  void cleanTag(std::string Tag, std::vector<std::string> &items);
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
  void downloadUser(std::string uid);
  void downloadSearchKeyword(std::string KW);
  void downloadTimeline();
  void downloadItemID(std::string item_id);
  void unlikeCached();
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
