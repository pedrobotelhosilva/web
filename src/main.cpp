#include "ConfigParser.hpp"
#include "Server.hpp"
#include <iostream>
#include <vector>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    std::string config_file = argv[1];
    ConfigParser parser(config_file);
    std::vector<std::map<std::string, std::string> > server_configs = parser.getConfig();

    Server server(server_configs);

    server.run();

    return 0;
}
