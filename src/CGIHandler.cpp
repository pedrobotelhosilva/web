#include "CGIHandler.hpp"
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include "Utils.hpp"

CGIHandler::CGIHandler(const std::string& script_path, const HTTPRequest& request)
    : script_path(script_path), request(request) {}

std::string CGIHandler::execute() {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        std::cerr << "pipe error: " << strerror(errno) << std::endl;
        return generateErrorResponse(500, "Internal Server Error");
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "fork error: " << strerror(errno) << std::endl;
        return generateErrorResponse(500, "Internal Server Error");
    }

    if (pid == 0) { // Child process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        setEnvironment();

        char* args[] = {const_cast<char*>("/usr/bin/php-cgi"), const_cast<char*>(script_path.c_str()), NULL};
        execvp(args[0], args);

        std::cerr << "execvp error: " << strerror(errno) << std::endl;
        exit(1);
    } else { // Parent process
        close(pipefd[1]);

        std::stringstream output;
        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            output.write(buffer, bytes_read);
        }

        close(pipefd[0]);
        waitpid(pid, NULL, 0);

        return formatHTTPResponse(output.str());
    }
}

void CGIHandler::setEnvironment() const {
    setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
    setenv("SCRIPT_FILENAME", script_path.c_str(), 1);
    setenv("QUERY_STRING", request.getQueryString().c_str(), 1);
    setenv("REQUEST_METHOD", request.getMethod().c_str(), 1);
    setenv("CONTENT_TYPE", request.getHeader("Content-Type").c_str(), 1);
    setenv("CONTENT_LENGTH", request.getHeader("Content-Length").c_str(), 1);
    setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
    setenv("REDIRECT_STATUS", "200", 1);
}

std::string CGIHandler::formatHTTPResponse(const std::string& cgi_output) const {
    std::string http_response = "HTTP/1.1 200 OK\r\n";
    http_response += "Content-Length: " + intToString(cgi_output.size()) + "\r\n";
    http_response += "Content-Type: text/html\r\n";
    http_response += "\r\n";
    http_response += cgi_output;
    return http_response;
}

std::string CGIHandler::generateErrorResponse(int status_code, const std::string& status_message) const {
    std::string http_response = "HTTP/1.1 " + intToString(status_code) + " " + status_message + "\r\n";
    http_response += "Content-Type: text/html\r\n";
    http_response += "\r\n";
    http_response += "<html><body><h1>" + intToString(status_code) + " " + status_message + "</h1></body></html>";
    return http_response;
}
