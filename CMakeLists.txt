cmake_minimum_required(VERSION 3.11)
project(BCY)
set (CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -O0 -D DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -D NDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -O2 -D DEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_RELEASE} -O3 -D NDEBUG")

option(SQLITECPP_INTERNAL_SQLITE "" OFF)
option(BUILD_CPR_TESTS "" OFF)
option(USE_SYSTEM_CURL "" ON)
add_compile_definitions("SQLITE_OMIT_LOAD_EXTENSION")
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/cpr EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp EXCLUDE_FROM_ALL)
find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED system filesystem thread regex program_options log)
find_library(CRYPTOPP_LIB cryptopp)
if(NOT CRYPTOPP_LIB)
  message(FATAL_ERROR "CryptoPP not found")
endif()
file(GLOB BCYCORE_SRC
    "include/Core.hpp"
    "src/Utils.cpp"
    "include/Base64.h"
    "include/Utils.hpp"
    "include/json.hpp"
    "src/Core.cpp"
)
file(GLOB BCYDOWNLOADUTILS_SRC
    "include/DownloadUtils.hpp"
    "include/DownloadFilter.hpp"
    "src/DownloadUtils.cpp"
    "src/DownloadFilter.cpp"
)
add_library(BCYCore STATIC ${BCYCORE_SRC})
add_library(BCYDownloadUtils STATIC ${BCYDOWNLOADUTILS_SRC})
add_executable(BCYDownloader utils/BCYDownloader.cpp)

set_target_properties(BCYCore PROPERTIES PUBLIC_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/BCY/Core.hpp;${CMAKE_CURRENT_LIST_DIR}/include/BCY/Utils.hpp;${CMAKE_CURRENT_LIST_DIR}/include/BCY/Base64.h;${CMAKE_CURRENT_LIST_DIR}/include/BCY/json.hpp")
target_include_directories(BCYCore PUBLIC ${BOOST_INCLUDE_DIRS} "/usr/local/include/" "/usr/include/" ${CMAKE_CURRENT_LIST_DIR}/include/ ${CPR_INCLUDE_DIRS})
target_link_libraries(BCYCore ${CRYPTOPP_LIB} ${CPR_LIBRARIES} ${Boost_LIBRARIES})

set_target_properties(BCYDownloadUtils PROPERTIES PUBLIC_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadFilter.hpp;${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadUtils.hpp")
target_include_directories(BCYDownloadUtils PUBLIC ${BOOST_INCLUDE_DIRS} "/usr/local/include/" "/usr/include/" ${CMAKE_CURRENT_LIST_DIR}/include/ ${CPR_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp/include)
target_link_libraries(BCYDownloadUtils BCYCore SQLiteCpp sqlite3 pthread z)

target_include_directories(BCYDownloader PUBLIC ${BOOST_INCLUDE_DIRS} "/usr/local/include/" "/usr/include/" ${CMAKE_CURRENT_LIST_DIR}/include/ ${CPR_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp/include)
target_link_libraries(BCYDownloader BCYCore BCYDownloadUtils)

install(TARGETS BCYDownloader DESTINATION bin COMPONENT BCYDownloader)
install(TARGETS BCYCore BCYDownloadUtils
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/BCY/
)
