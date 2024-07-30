#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HTTPResponse {
public:
    HTTPResponse();
    void setStatusCode(int code);
    void setStatusMessage(const std::string& message);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    std::string toString() const;

private:
    int statusCode;
    std::string statusMessage;
    std::map<std::string, std::string> headers;
    std::string body;
};

#endif
