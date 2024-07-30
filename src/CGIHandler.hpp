#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include <string>
#include "HTTPRequest.hpp"

class CGIHandler {
public:
    CGIHandler(const std::string& script_path, const HTTPRequest& request);
    std::string execute();

private:
    std::string script_path;
    HTTPRequest request;

    void setEnvironment() const;
    std::string formatHTTPResponse(const std::string& cgi_output) const;
    std::string generateErrorResponse(int status_code, const std::string& status_message) const;
};

#endif
