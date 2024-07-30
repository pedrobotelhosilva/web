#include "HTTPResponse.hpp"
#include <sstream>

HTTPResponse::HTTPResponse() : statusCode(200), statusMessage("OK") {}

void HTTPResponse::setStatusCode(int code) {
    statusCode = code;
}

void HTTPResponse::setStatusMessage(const std::string& message) {
    statusMessage = message;
}

void HTTPResponse::setHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
}

void HTTPResponse::setBody(const std::string& body) {
    this->body = body;
}

std::string HTTPResponse::toString() const {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusMessage << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        response << it->first << ": " << it->second << "\r\n";
    }
    response << "\r\n" << body;
    return response.str();
}
