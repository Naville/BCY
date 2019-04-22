#ifndef BCY_UTILS_HPP
#define BCY_UTILS_HPP
#include <cpprest/json.h>
#include <sstream>
#include <string>
namespace BCY {
std::string bcy_string_to_hex(const std::string &input);//Thank you OpenSSL for polluting my namespace
std::string generateRandomString(std::string alphabet, size_t length);
std::string expand_user(std::string path);
std::string ensure_string(web::json::value foo);
template <typename InputIt>
std::string join(InputIt begin, InputIt end,
                 const std::string &separator = ", ",
                 const std::string &concluder = "")
{
  std::ostringstream ss;

  if (begin != end) {
    ss << *begin++;
  }

  while (begin != end)
  {
    ss << separator;
    ss << *begin++;
  }

  ss << concluder;
  return ss.str();
}
} // namespace BCY
#endif
