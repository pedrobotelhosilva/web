#include "Utils.hpp"
#include <sstream>

char hexToChar(const std::string& hex) {
    int intValue;
    std::istringstream iss(hex);
    iss >> std::hex >> intValue;
    return static_cast<char>(intValue);
}

std::string intToString(int num) {
    std::ostringstream oss;
    oss << num;
    return oss.str();
}
