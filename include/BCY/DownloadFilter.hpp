#ifndef BCY_DOWNLOADFILTER_HPP
#define BCY_DOWNLOADFILTER_HPP
#include <SQLiteCpp/SQLiteCpp.h>
#include <cpprest/json.h>
#include <BCY/DownloadUtils.hpp>
namespace BCY {
enum class ScriptEvalMode {
    Abstract = 0,
    Detail,
    Tags,
};
typedef std::function<int(DownloadUtils::Info&)> BCYFilterHandler;
class DownloadFilter {
public:
  DownloadFilter() = delete;
  DownloadFilter(DownloadFilter &) = delete;
  ~DownloadFilter();
  DownloadFilter(std::string DBPath);
  void loadRulesFromJSON(web::json::value rules);
  void addFilterHandler(BCYFilterHandler handle);
  bool shouldBlockItem(DownloadUtils::Info&);
  bool shouldBlockAbstract(web::json::value&);
  std::vector<std::string> UIDList;
  std::vector<std::string> TagList;
  std::vector<std::string> UserNameList;
  std::vector<std::string> ItemList;
  // Each should return an integer value. > 0 for allow, =0 for
  // defer, <0 for deny.
  std::vector<std::string> ScriptList;
private:
  std::vector<BCYFilterHandler> filterHandlers;
  SQLite::Database *DB = nullptr;
  std::string DBPath;
};
} // namespace BCY
#endif
