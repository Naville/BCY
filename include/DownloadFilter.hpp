#ifndef BCY_DOWNLOADFILTER_HPP
#define BCY_DOWNLOADFILTER_HPP
#include "json.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
using json = nlohmann::json;
typedef std::function<int(json &)> BCYFilterHandler;
namespace BCY {
class BCYDownloadFilter {
public:
  BCYDownloadFilter() = delete;
  ~BCYDownloadFilter();
  BCYDownloadFilter(SQLite::Database *DB);
  SQLite::Database *DB = nullptr;
  bool shouldBlock(json abstract);
  void loadRulesFromJSON(json rules);
  void addFilterHandler(BCYFilterHandler handle);
  std::vector<std::string> UIDList;
  std::vector<std::string> WorkList;
  std::vector<std::string> TagList;
  std::vector<std::string> UserNameList;
  std::vector<std::string> TypeList;

private:
  std::vector<BCYFilterHandler> FilterHandlers;
};
} // namespace BCY
#endif
