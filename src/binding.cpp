#include "BCY/BCY.h"
#include <boost/python.hpp>
#include <chaiscript/chaiscript.hpp>
#include <string>
using namespace BCY;
using namespace std;
using namespace boost::python;
using namespace nlohmann;
BOOST_PYTHON_MODULE(PyBCY) {
  auto cor = class_<Core, boost::noncopyable>("BCYCore");
  auto du = class_<DownloadUtils, boost::noncopyable>(
      "BCYDownloadUtils", init<string, int, int, string>());
  auto df = class_<DownloadFilter, boost::noncopyable>("BCYDownloadFilter",
                                                       init<string>());
  df.def("shouldBlock", +[](DownloadFilter &obj, string &arg) {
    return obj.shouldBlock(json::parse(arg));
  });
  df.def("loadRulesFromJSON", +[](DownloadFilter &obj, string &arg) {
    return obj.loadRulesFromJSON(json::parse(arg));
  });
  cor.def("EncryptData", &Core::EncryptData);
  cor.def("EncryptParam", +[](Core &obj, string &arg) {
    json j = json::parse(arg);
    json k = obj.EncryptParam(j);
    return k.dump();
  });
}
