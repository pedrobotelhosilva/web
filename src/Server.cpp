#include "Server.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "CGIHandler.hpp"
#include "Utils.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <algorithm>
#include <sys/wait.h>
#include <dirent.h>

static void log(std::string const& s) {
    std::cout << s << std::endl;
}

Server::Server(const std::vector<std::map<std::string, std::string> >& configs) : configs(configs) {
    FD_ZERO(&master_set);
    FD_ZERO(&read_set);
    max_fd = 0;
    init();
}

void Server::init() {
    for (std::vector<std::map<std::string, std::string> >::const_iterator it = configs.begin(); it != configs.end(); ++it) {
        createSocket(*it);
    }
}

void Server::createSocket(const std::map<std::string, std::string>& config) {
    std::string portStr = config.at("listen");
    int portNum = atoi(portStr.c_str());
    if (portNum <= 0) {
        throw std::runtime_error("Invalid port number: " + portStr);
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Failed to set socket options");
    }

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(portNum);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Failed to bind socket on port " + intToString(portNum));
    }

    if (listen(listen_fd, 10) < 0) {
        throw std::runtime_error("Failed to listen on socket on port " + intToString(portNum));
    }

    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    FD_SET(listen_fd, &master_set);
    if (listen_fd > max_fd) {
        max_fd = listen_fd;
    }

    listen_fds.push_back(listen_fd);

    std::cout << "Server initialized and listening on port " << portNum << "..." << std::endl;
}

void Server::run() {
    while (true) {
        read_set = master_set;
        int activity = select(max_fd + 1, &read_set, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            std::cerr << "Select error: " << strerror(errno) << std::endl;
            return;
        }

        for (int i = 0; i <= max_fd; ++i) {
            if (FD_ISSET(i, &read_set)) {
                if (std::find(listen_fds.begin(), listen_fds.end(), i) != listen_fds.end()) {
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(i, (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd < 0) {
                        std::cerr << "Accept error on fd " << i << ": " << strerror(errno) << std::endl;
                        continue;
                    }

                    fcntl(client_fd, F_SETFL, O_NONBLOCK);
                    FD_SET(client_fd, &master_set);
                    if (client_fd > max_fd) {
                        max_fd = client_fd;
                    }

                    std::cout << "New connection accepted on port " << ntohs(client_addr.sin_port) << ", fd: " << client_fd << std::endl;
                } else {
                    // Encontrar a configuração associada ao descritor do cliente
                    std::map<std::string, std::string> config;
                    for (std::vector<std::map<std::string, std::string> >::const_iterator it = configs.begin(); it != configs.end(); ++it) {
                        std::string portStr = it->at("listen");
                        int portNum = atoi(portStr.c_str());
                        sockaddr_in addr;
                        socklen_t len = sizeof(addr);
                        getsockname(i, (struct sockaddr*)&addr, &len);
                        if (ntohs(addr.sin_port) == portNum) {
                            config = *it;
                            break;
                        }
                    }

                    if (!config.empty()) {
                        std::cout << "Handling request for fd: " << i << std::endl;
                        handleRequest(i, config);
                        std::cout << "Request handled for fd: " << i << std::endl;
                        close(i);
                        FD_CLR(i, &master_set);
                        std::cout << "Connection closed and fd removed: " << i << std::endl;
                    }
                }
            }
        }
    }
}

void Server::handleRequest(int client_fd, const std::map<std::string, std::string>& config) {
    char buffer[4096];
    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read < 0) {
        std::cerr << "Error reading from socket\n";
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';
    std::string raw_request(buffer);
    HTTPRequest request(raw_request);

    std::string method = request.getMethod();
    std::string uri = request.getPath();
    std::string requested_path = getRequestedPath(uri, config);

    if (uri == "/old-path") {
        HTTPResponse response;
        response.setStatusCode(301);
        response.setStatusMessage("Moved Permanently");
        response.setHeader("Location", "/new-path");
        send(client_fd, response.toString().c_str(), response.toString().length(), 0);
        close(client_fd);
        return;
    }

    if (uri == "/custom-return") {
        HTTPResponse response;
        response.setStatusCode(200);
        response.setStatusMessage("OK");
        response.setBody("Custom return message");
        send(client_fd, response.toString().c_str(), response.toString().length(), 0);
        close(client_fd);
        return;
    }

    if (!isMethodAllowed(uri, method, config)) {
        sendErrorResponse(client_fd, 405, "Method Not Allowed");
        return;
    }

    if (method == "POST") {
        
        std::string boundary = "--" + request.getHeader("Content-Type").substr(30);
        size_t filename_pos = raw_request.find("filename=\"") + 10;
        size_t filename_end_pos = raw_request.find("\"", filename_pos);
        std::string filename = raw_request.substr(filename_pos, filename_end_pos - filename_pos);

        size_t file_content_start = raw_request.find("\r\n\r\n", filename_end_pos) + 4;
        size_t file_content_end = raw_request.find(boundary, file_content_start) - 4;

        std::string file_content = raw_request.substr(file_content_start, file_content_end - file_content_start);

        std::ofstream outfile((config.at("root") + "/upload/" + filename).c_str());
        outfile << file_content;
        outfile.close();

        HTTPResponse response;
        response.setStatusCode(200);
        response.setStatusMessage("OK");
        if (uri == "/upload")
            response.setBody("<html><body><h1>File Uploaded</h1></body></html>");
        else
            response.setBody("<html><body><h1>POST request successful</h1></body></html>");
        send(client_fd, response.toString().c_str(), response.toString().length(), 0);
        close(client_fd);
        return;
    }

    if (method == "DELETE") {
        std::string decoded_path = uri;
        size_t pos = decoded_path.find('%');
        while (pos != std::string::npos) {
            if (pos + 2 < decoded_path.length()) {
                std::string hex_str = decoded_path.substr(pos + 1, 2);
                char decoded_char = static_cast<char>(strtol(hex_str.c_str(), NULL, 16));
                decoded_path.replace(pos, 3, 1, decoded_char);
            }
            pos = decoded_path.find('%', pos + 1);
        }
        requested_path = config.at("root") + decoded_path;

        if (remove(requested_path.c_str()) == 0) {
            HTTPResponse response;
            response.setStatusCode(200);
            response.setStatusMessage("OK");
            response.setBody("<html><body><h1>File Deleted</h1></body></html>");
            send(client_fd, response.toString().c_str(), response.toString().length(), 0);
        } else {
            sendErrorResponse(client_fd, 404, "Not Found");
        }
        close(client_fd);
        return;
    }

    if (method == "GET" && uri == "/cause500") {
        sendErrorResponse(client_fd, 500, "Internal Server Error");
        close(client_fd);
        return;
    }

    if (method == "GET") {
        struct stat file_stat;
        if (stat(requested_path.c_str(), &file_stat) == -1) {
            sendErrorResponse(client_fd, 404, "Not Found");
        }

        else if (S_ISDIR(file_stat.st_mode)) {
            if (requested_path.at(requested_path.size()-1) != '/') {
                sendRedirectResponse(client_fd, uri+"/");
            }
            std::string index_path = requested_path + "index.html";
            if (stat(index_path.c_str(), &file_stat) == -1) {
                sendDirListing(client_fd, uri, requested_path);
                close(client_fd);
                return;
            }
            serveFile(client_fd, index_path);
        }
        else if (requested_path.find(".php") != std::string::npos) {
            handleCGI(client_fd, requested_path, request);
        } else {
            serveFile(client_fd, requested_path);
        }
    } else {
        sendErrorResponse(client_fd, 405, "Method Not Allowed");
    }
    close(client_fd);
}

void Server::sendRedirectResponse(int client_fd, const std::string& normalized_url) {
    HTTPResponse response;
    response.setStatusCode(301);
    response.setStatusMessage("Moved Permanently");
    response.setHeader("Location", normalized_url);
    response.setHeader("Content-Length", "0");

    std::string raw_response = response.toString();
    if (write(client_fd, raw_response.c_str(), raw_response.size()) < 0) {
        std::cerr << "Error writing error response to fd: " << client_fd << ", error: " << strerror(errno) << std::endl;
    }
}

std::string Server::getRequestedPath(const std::string& uri, const std::map<std::string, std::string>& config) {
    std::map<std::string, std::string>::const_iterator it = config.find("location " + uri);
    if (it != config.end()) {
        return it->second;
    }
    return config.at("root") + uri;
}

bool Server::isMethodAllowed(const std::string& uri, const std::string& method, const std::map<std::string, std::string>& config) {

    std::map<std::string, std::string>::const_iterator it = config.find("location " + uri + " allow_methods");
    
    if (it != config.end()) {
        std::istringstream ss(it->second);
        std::string allowed_method;

        while (ss >> allowed_method) {
            if (allowed_method == method) {
                return true;
            }
        }
        return false;
    }
    return true;
}

void Server::serveFile(int client_fd, const std::string& path) {
    std::string content = readFile(path);
    if (content.empty()) {
        sendErrorResponse(client_fd, 404, "Not Found");
        return;
    }

    HTTPResponse response;
    response.setStatusCode(200);
    response.setStatusMessage("OK");
    response.setHeader("Content-Type", getMimeType(path));
    response.setBody(content);

    std::string raw_response = response.toString();
    if (write(client_fd, raw_response.c_str(), raw_response.size()) < 0) {
        std::cerr << "Error writing response to fd: " << client_fd << ", error: " << strerror(errno) << std::endl;
    }
}

void Server::sendErrorResponse(int client_fd, int status_code, const std::string& status_message) {
    HTTPResponse response;
    response.setStatusCode(status_code);
    response.setStatusMessage(status_message);
    response.setHeader("Content-Type", "text/html");
    response.setBody("<html><body><h1>" + intToString(status_code) + " " + status_message + "</h1></body></html>");

    std::string raw_response = response.toString();
    if (write(client_fd, raw_response.c_str(), raw_response.size()) < 0) {
        std::cerr << "Error writing error response to fd: " << client_fd << ", error: " << strerror(errno) << std::endl;
    }
}

void Server::sendDirListing(int client_fd, const std::string& uri, const std::string& path) {
    HTTPResponse response;
    response.setStatusCode(200);
    response.setStatusMessage("OK");
    response.setHeader("Content-Type", "text/html");

    DIR* dir;
    struct dirent* entry;
    std::ostringstream oss_body;

    oss_body <<
        "<html>"
        "<head><title>Directory Listing</title></head>"
        "<body>"
        "<h1>Directory listing for "
        << uri + "</h1>"
        "<hr>"
        "<ul>";

    if ((dir = opendir(path.c_str())) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            // Skip . and .. dirs
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue ;
            }
            std::string entry_path = path + entry->d_name;

            struct stat stat_buf;
            if (stat(entry_path.c_str(), &stat_buf) == -1) {
                log("Error accessing file information from dir listing");
            }

            oss_body
                << "<li>"
                << "<a href=\""
                << uri + entry->d_name;
            if (S_ISDIR(stat_buf.st_mode)) {
                oss_body << "/";
            }
            oss_body
                << "\">"
                << entry->d_name
                << "</a></li>";
        }
        closedir(dir);
    }
    oss_body << "</ul><hr></body></html>";

    response.setBody(oss_body.str());

    std::string raw_response = response.toString();
    if (write(client_fd, raw_response.c_str(), raw_response.size()) < 0) {
        std::cerr << "Error writing error response to fd: " << client_fd << ", error: " << strerror(errno) << std::endl;
    }
}

std::string Server::getMimeType(const std::string& path) {
    if (path.find(".html") != std::string::npos || path.find(".htm") != std::string::npos) return "text/html";
    if (path.find(".css") != std::string::npos) return "text/css";
    if (path.find(".js") != std::string::npos) return "application/javascript";
    if (path.find(".png") != std::string::npos) return "image/png";
    if (path.find(".svg") != std::string::npos) return "image/svg+xml";
    if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) return "image/jpeg";
    if (path.find(".gif") != std::string::npos) return "image/gif";
    if (path.find(".txt") != std::string::npos) return "text/plain";
    return "application/octet-stream";
}

std::string Server::readFile(const std::string& path) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string Server::intToString(int num) {
    std::ostringstream oss;
    oss << num;
    return oss.str();
}

void Server::handleCGI(int client_fd, const std::string& scriptPath, const HTTPRequest& request) {
    int pid = fork();
    if (pid == -1) {
        sendErrorResponse(client_fd, 500, "Internal Server Error");
        return;
    }
    if (pid == 0) {
        CGIHandler cgiHandler(scriptPath, request);
        std::string cgi_output = cgiHandler.execute();
        send(client_fd, cgi_output.c_str(), cgi_output.length(), 0);
        close(client_fd);
        exit(0);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}
