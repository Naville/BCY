#ifndef BCY_DOWNLOADFILTER_HPP
#define BCY_DOWNLOADFILTER_HPP
#include "json.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
typedef std::function<int(nlohmann::json &)> BCYFilterHandler;
namespace BCY {
    class BCYDownloadFilter {
    public:
        BCYDownloadFilter() = delete;
        ~BCYDownloadFilter();
        BCYDownloadFilter(std::string DBPath);
        SQLite::Database *DB = nullptr;
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
        std::string DBPath;
    };
} // namespace BCY
#endif
