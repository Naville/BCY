#ifndef BCY_UTILS_HPP
#define BCY_UTILS_HPP
#include <sstream>
#include <string>
#include "json.hpp"
namespace BCY {
std::string string_to_hex(const std::string &input);
std::string generateRandomString(std::string alphabet, size_t length);
std::string expand_user(std::string path);
template <typename InputIt>
std::string join(InputIt begin, InputIt end,
                 const std::string &separator = ", ", // see 1.
                 const std::string &concluder = "")   // see 1.
{
  std::ostringstream ss;

  if (begin != end) {
    ss << *begin++; // see 3.
  }

  while (begin != end) // see 3.
  {
    ss << separator;
    ss << *begin++;
  }

  ss << concluder;
  return ss.str();
}
} // namespace BCY
#endif
