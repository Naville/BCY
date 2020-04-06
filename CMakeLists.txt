cmake_minimum_required(VERSION 3.11)
project(BCY)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -O0 -D DEBUG -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -D NDEBUG -Wall")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -O2 -D DEBUG -Wall")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_RELEASE} -O3 -D NDEBUG -Wall")
option(SQLITECPP_INTERNAL_SQLITE "" OFF)
option(BCY_BUILD_DOCS "Generate Documentation" OFF)
add_compile_definitions("SQLITE_OMIT_LOAD_EXTENSION")
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp EXCLUDE_FROM_ALL)
find_package(PkgConfig REQUIRED)
SET(Boost_USE_STATIC_LIBS ON)
SET(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.67.0 REQUIRED COMPONENTS system filesystem thread regex program_options log)
find_library(CRYPTOPP_LIB cryptopp)
find_package(cpprestsdk REQUIRED)
if(NOT CRYPTOPP_LIB)
  message(FATAL_ERROR "CryptoPP not found")
endif()
file(GLOB BCYCORE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/include/BCY/Core.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/Utils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/BCY/Base64.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/BCY/Utils.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/Core.cpp"
)
file(GLOB BCYDOWNLOADUTILS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadUtils.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadFilter.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/DownloadUtils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/DownloadFilter.cpp"
)
add_library(BCYCore STATIC ${BCYCORE_SRC})
add_library(BCYDownloadUtils STATIC ${BCYDOWNLOADUTILS_SRC})
add_executable(BCYDownloader tools/BCYDownloader.cpp)

include_directories(AFTER SYSTEM ${CMAKE_CURRENT_LIST_DIR}/ChaiScript/include/ ${BOOST_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/include/ "/usr/local/include/" "/usr/include/")

set_target_properties(BCYCore PROPERTIES PUBLIC_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/BCY/Core.hpp;${CMAKE_CURRENT_LIST_DIR}/include/BCY/Utils.hpp;${CMAKE_CURRENT_LIST_DIR}/include/BCY/Base64.h")
target_link_libraries(BCYCore ${CRYPTOPP_LIB} Boost::system Boost::filesystem Boost::thread Boost::regex Boost::program_options Boost::log cpprestsdk::cpprest)

set_target_properties(BCYDownloadUtils PROPERTIES PUBLIC_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadFilter.hpp;${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadUtils.hpp")
target_include_directories(BCYDownloadUtils PUBLIC ${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp/include)
target_link_libraries(BCYDownloadUtils BCYCore SQLiteCpp sqlite3 pthread z)

target_include_directories(BCYDownloader PUBLIC ${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp/include)
target_link_libraries(BCYDownloader BCYDownloadUtils)

IF(UNIX)
  target_link_libraries(BCYDownloader -ldl -lcrypto)
ENDIF()

install(TARGETS BCYDownloader DESTINATION bin COMPONENT BCYDownloader)
install(TARGETS BCYCore BCYDownloadUtils
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/BCY/
)

IF(BCY_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  SET(BCY_HEADERS_TO_GEN_DOCS
    "${CMAKE_CURRENT_LIST_DIR}/include/BCY/Core.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/BCY/Utils.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadUtils.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadFilter.hpp"
  )
  doxygen_add_docs(BCYDOCS ${BCY_HEADERS_TO_GEN_DOCS} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ENDIF()
