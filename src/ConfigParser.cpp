#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

ConfigParser::ConfigParser(const std::string& filename) {
    parseConfig(filename);
}

std::vector<std::map<std::string, std::string> > ConfigParser::getConfig() const {
    return config;
}

void ConfigParser::parseConfig(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file");
    }

    std::string line;
    std::map<std::string, std::string> server_config;
    bool in_server_block = false;
    bool in_location_block = false;
    std::string current_location;

    while (std::getline(file, line)) {
        trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line == "server {") {
            if (in_server_block) {
                throw std::runtime_error("Nested server block detected");
            }
            in_server_block = true;
            server_config.clear();
            continue;
        } else if (line == "}") {
            if (in_location_block) {
                in_location_block = false;
                current_location.clear();
                continue;
            }
            if (in_server_block) {
                in_server_block = false;
                config.push_back(server_config);
                continue;
            }
            throw std::runtime_error("Unmatched closing brace");
        } else if (line.find("location") == 0 && line[line.size() - 1] == '{') {
            if (!in_server_block) {
                throw std::runtime_error("Location block outside server block");
            }
            if (in_location_block) {
                throw std::runtime_error("Nested location block detected");
            }
            in_location_block = true;
            size_t pos = line.find(' ');
            current_location = line.substr(pos + 1, line.length() - pos - 2);
            trim(current_location);
            continue;
        }

        size_t delimiter_pos = line.find_first_of(" =");
        if (delimiter_pos == std::string::npos) {
            throw std::runtime_error("Invalid config line: " + line);
        }

        std::string key = line.substr(0, delimiter_pos);
        std::string value = line.substr(delimiter_pos + 1);

        trim(key);
        trim(value);

        // Remove trailing semicolon if present
        if (!value.empty() && value[value.size() - 1] == ';') {
            value.erase(value.size() - 1);
        }

        trim(value);

        if (in_location_block) {
            key = "location " + current_location + " " + key;
        }

        server_config[key] = value;
    }

    if (in_server_block) {
        throw std::runtime_error("Unclosed server block");
    }
}

void ConfigParser::trim(std::string& str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    str.erase(std::find_if(str.rbegin(), str.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), str.end());
}
