cmake_minimum_required(VERSION 3.11)
project(BCY)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -O0 -D DEBUG -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -D NDEBUG -Wall")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -O2 -D DEBUG -Wall")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_RELEASE} -O3 -D NDEBUG -Wall")

option(SQLITECPP_INTERNAL_SQLITE "" OFF)
option(ENABLE_PYTHON_BINDING "Enable PyBCY" OFF)
set(PYTHON_VERSION "3.7" CACHE STRING "Python Version Used For Python Binding" )
add_compile_definitions("SQLITE_OMIT_LOAD_EXTENSION")
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp EXCLUDE_FROM_ALL)
find_package(PkgConfig REQUIRED)
find_package(Boost 1.67.0 REQUIRED system filesystem thread regex program_options log)
find_library(CRYPTOPP_LIB cryptopp)
find_package(cpprestsdk REQUIRED)
if(NOT CRYPTOPP_LIB)
  message(FATAL_ERROR "CryptoPP not found")
endif()
file(GLOB BCYCORE_SRC
    "include/Core.hpp"
    "src/Utils.cpp"
    "include/Base64.h"
    "include/Utils.hpp"
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

include_directories(AFTER SYSTEM ${CMAKE_CURRENT_LIST_DIR}/ChaiScript/include/ ${BOOST_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/include/ "/usr/local/include/" "/usr/include/")

set_target_properties(BCYCore PROPERTIES PUBLIC_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/BCY/Core.hpp;${CMAKE_CURRENT_LIST_DIR}/include/BCY/Utils.hpp;${CMAKE_CURRENT_LIST_DIR}/include/BCY/Base64.h")
target_link_libraries(BCYCore ${CRYPTOPP_LIB} ${Boost_LIBRARIES} cpprestsdk::cpprest)

set_target_properties(BCYDownloadUtils PROPERTIES PUBLIC_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadFilter.hpp;${CMAKE_CURRENT_LIST_DIR}/include/BCY/DownloadUtils.hpp")
target_include_directories(BCYDownloadUtils PUBLIC ${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp/include)
target_link_libraries(BCYDownloadUtils BCYCore SQLiteCpp sqlite3 pthread z)

target_include_directories(BCYDownloader PUBLIC ${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp/include)
target_link_libraries(BCYDownloader BCYDownloadUtils)

install(TARGETS BCYDownloader DESTINATION bin COMPONENT BCYDownloader)
install(TARGETS BCYCore BCYDownloadUtils
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/BCY/
)

IF(ENABLE_PYTHON_BINDING)
message(STATUS "Building For Python${PYTHON_VERSION}")
add_library(PyBCY SHARED src/binding.cpp)
find_package(PythonLibs ${PYTHON_VERSION} REQUIRED)
STRING(REPLACE "." "" PYTHON_VERSION_NODOT ${PYTHON_VERSION})
find_package(Boost 1.67.0 REQUIRED COMPONENTS system filesystem thread regex program_options log python${PYTHON_VERSION_NODOT})
# Suppress prefix "lib" because Python does not allow this prefix
set_target_properties(PyBCY PROPERTIES PREFIX "")
IF(WIN32)
  set_target_properties(PyBCY PROPERTIES SUFFIX ".dll")
ELSE()
  set_target_properties(PyBCY PROPERTIES SUFFIX ".so")
ENDIF()
find_package(Python ${PYTHON_VERSION} REQUIRED COMPONENTS Interpreter Development)
target_include_directories(PyBCY PUBLIC ${BOOST_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/include/ "/usr/local/include/" "/usr/include/" ${PYTHON_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/SQLiteCpp/include ${Python_INCLUDE_DIRS})
target_link_libraries(PyBCY ${CRYPTOPP_LIB} ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} BCYCore BCYDownloadUtils)
message(STATUS "Installing PyBCY to:"${Python_SITEARCH})
install(TARGETS PyBCY DESTINATION "${Python_SITEARCH}/")
ENDIF()
