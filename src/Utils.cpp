#include "BCY/Utils.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
using json = web::json::value;
using namespace std;
string BCY::string_to_hex(const string &input) {
  static const char *const lut = "0123456789abcdef";
  size_t len = input.length();

  std::string output;
  output.reserve(2 * len);
  for (size_t i = 0; i < len; ++i) {
    const unsigned char c = input[i];
    output.push_back(lut[c >> 4]);
    output.push_back(lut[c & 15]);
  }
  return output;
}
string BCY::ensure_string(json foo) {
  if (foo.is_string()) {
    return foo.as_string();
  } else if (foo.is_number()) {
    int num = foo.as_integer();
    return to_string(num);
  } else if (foo.is_null()) {
    return "";
  } else {
    throw std::invalid_argument(foo.serialize() +
                                " Can't Be Converted to String");
  }
}
string BCY::generateRandomString(string alphabet, size_t length) {
  stringstream ss;
  random_device rd;                // obtain a random number from hardware
  default_random_engine eng(rd()); // seed the generator
  uniform_int_distribution<> distr(0,
                                   alphabet.length() - 1); // define the range
  for (size_t i = 0; i < length; i++) {
    ss << alphabet[distr(eng)];
  }
  return ss.str();
}
string BCY::expand_user(string path) {
  if (not path.empty() and path[0] == '~') {
    assert(path.size() == 1 or path[1] == '/'); // or other error handling
    char const *home = getenv("HOME");
    if (home or ((home = getenv("USERPROFILE")))) {
      path.replace(0, 1, home);
    } else {
      char const *hdrive = getenv("HOMEDRIVE"), *hpath = getenv("HOMEPATH");
      assert(hdrive); // or other error handling
      assert(hpath);
      path.replace(0, 1, std::string(hdrive) + hpath);
    }
  }
  return path;
}
