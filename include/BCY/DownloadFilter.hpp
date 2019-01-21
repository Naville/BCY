#ifndef BCY_DOWNLOADFILTER_HPP
#define BCY_DOWNLOADFILTER_HPP
#include <SQLiteCpp/SQLiteCpp.h>
#include <cpprest/json.h>
typedef std::function<int(web::json::value &)> BCYFilterHandler;
namespace BCY {
class DownloadFilter {
public:
  DownloadFilter() = delete;
  DownloadFilter(DownloadFilter &) = delete;
  ~DownloadFilter();
  DownloadFilter(std::string DBPath);
  bool shouldBlockDetail(web::json::value detail);
bool shouldBlockAbstract(web::json::value abstract);
  void loadRulesFromJSON(web::json::value rules);
  void addFilterHandler(BCYFilterHandler handle);
  std::vector<web::json::value> UIDList;
  std::vector<web::json::value> WorkList;
  std::vector<web::json::value> TagList;
  std::vector<web::json::value> UserNameList;
  std::vector<web::json::value> TypeList;

private:
  std::vector<BCYFilterHandler> FilterHandlers;
  SQLite::Database *DB = nullptr;
  std::string DBPath;
};
} // namespace BCY
#endif
