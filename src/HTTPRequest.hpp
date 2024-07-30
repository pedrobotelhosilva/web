#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HTTPRequest {
public:
    HTTPRequest(const std::string& raw_request);
    std::string getMethod() const;
    std::string getPath() const;
    std::string getQueryString() const;
    std::string getHeader(const std::string& header_name) const;

private:
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    void parseRequest(const std::string& raw_request);
};

#endif
