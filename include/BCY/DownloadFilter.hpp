#ifndef BCY_DOWNLOADFILTER_HPP
#define BCY_DOWNLOADFILTER_HPP
#include "json.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
typedef std::function<int(nlohmann::json &)> BCYFilterHandler;
namespace BCY {
    class DownloadFilter {
    public:
        DownloadFilter() = delete;
        DownloadFilter(DownloadFilter &) = delete;
        ~DownloadFilter();
        DownloadFilter(std::string DBPath);
        bool shouldBlock(nlohmann::json abstract);
        void loadRulesFromJSON(nlohmann::json rules);
        void addFilterHandler(BCYFilterHandler handle);
        std::vector<std::string> UIDList;
        std::vector<std::string> WorkList;
        std::vector<std::string> TagList;
        std::vector<std::string> UserNameList;
        std::vector<std::string> TypeList;

    private:
        std::vector<BCYFilterHandler> FilterHandlers;
        SQLite::Database *DB = nullptr;
        std::string DBPath;
    };
} // namespace BCY
#endif
