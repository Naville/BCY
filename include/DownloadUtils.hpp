#ifndef BCY_DOWNLOADUTILS_HPP
#define BCY_DOWNLOADUTILS_HPP
#include "Core.hpp"
#include "DownloadFilter.hpp"
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
  BCYDownloadFilter *filter = nullptr;
  SQLite::Database *DB = nullptr;
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
                int downloadThreadCount = -1); //-1 to use hardware thread count
  void downloadFromAbstractInfo(json Inf);
  void downloadFromInfo(json Inf, bool save = true);
  std::string loadTitle(std::string title, json Inf);
  void saveInfo(std::string title, json Inf);
  json loadInfo(std::string item_id);
  std::string loadOrSaveGroupName(std::string name, std::string GID);
  void insertRecordForCompressedImage(std::string item_id);
  void verify();
  void cleanup();
  void join();
  void addFilter(std::string filter);
  void downloadGroupID(std::string gid);
  void downloadWorkID(std::string item);
  void downloadUserLiked(std::string uid);
  void downloadLiked();
  void downloadTag(std::string TagName);
  void downloadUser(std::string uid);
  void downloadSearchKeyword(std::string KW);
  void downloadTimeline();
  void downloadItemID(std::string item_id);

private:
  std::string md5(std::string &str);
  boost::asio::thread_pool *queryThread = nullptr;
  boost::asio::thread_pool *downloadThread = nullptr;
  std::mutex dbLock;
  std::string saveRoot;
  cpr::Session Sess;
  std::mutex sessLock;
  std::set<std::string> Filters;
  std::function<bool(json)> downloadCallback = [&](json j) {
    this->downloadFromAbstractInfo(j);
    return true;
  };
  bool stop = false;
};
} // namespace BCY
#endif
