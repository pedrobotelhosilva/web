#include "HTTPRequest.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cctype>

HTTPRequest::HTTPRequest(const std::string& raw_request) {
    parseRequest(raw_request);
}

void HTTPRequest::parseRequest(const std::string& raw_request) {
    std::istringstream request_stream(raw_request);
    std::string line;
    std::getline(request_stream, line);

    std::istringstream line_stream(line);
    line_stream >> method >> path;
    
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        query_string = path.substr(query_pos + 1);
        path = path.substr(0, query_pos);
    }

    std::cout << "Parsed Method: " << method << ", Parsed Path: " << path << ", Query String: " << query_string << std::endl;

    // Parse headers (optional for this example)
    while (std::getline(request_stream, line) && line != "\r") {
        std::string header_name;
        std::string header_value;
        std::istringstream header_stream(line);
        std::getline(header_stream, header_name, ':');
        std::getline(header_stream, header_value);
        header_value.erase(header_value.begin(), std::find_if(header_value.begin(), header_value.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        headers[header_name] = header_value;
    }
}

std::string HTTPRequest::getMethod() const {
    return method;
}

std::string HTTPRequest::getPath() const {
    return path;
}

std::string HTTPRequest::getQueryString() const {
    return query_string;
}

std::string HTTPRequest::getHeader(const std::string& header_name) const {
    std::map<std::string, std::string>::const_iterator it = headers.find(header_name);
    if (it != headers.end()) {
        return it->second;
    } else {
        return "";
    }
}
