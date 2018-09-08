#include "Utils.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <json.hpp>
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
