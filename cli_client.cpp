#include "Protocol.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

std::string resolve_absolute_path(const std::string& cwd, const std::string& arg) {
    if (!arg.empty() && arg[0] == '/') return arg;
    if (cwd == "/") return cwd + arg;
    return cwd + "/" + arg;
}

std::string simplify_path(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string token;
    while (getline(ss, token, '/')) {
        if (token == "" || token == ".") continue;
        if (token == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(token);
        }
    }
    std::string res = "";
    for (const auto& p : parts) res += "/" + p;
    return res.empty() ? "/" : res;
}

void send_command(int fd, CommandType type, const std::string& path, const std::vector<char>& data = {}) {
    RequestHeader req;
    req.type = type;
    req.path_len = path.length();
    req.data_len = data.size();
    
    write(fd, &req, sizeof(req));
    if (req.path_len > 0) write(fd, path.c_str(), req.path_len);
    if (req.data_len > 0) write(fd, data.data(), req.data_len);
    
    ResponseHeader res;
    read(fd, &res, sizeof(res));
    
    if (res.status == 0) {
        if (res.data_len > 0) {
            std::vector<char> res_data(res.data_len + 1, 0);
            read(fd, res_data.data(), res.data_len);
            std::cout << res_data.data();
            if (res_data[res.data_len - 1] != '\n' && type != CMD_READ) std::cout << "\n";
        } else {
            std::cout << "Success.\n";
        }
    } else {
        std::cerr << "Error: operation failed.\n";
    }
}

int main() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        return 1;
    }
    
    std::string cwd = "/";
    std::string line;
    
    std::cout << "Connected to VFS server. Type 'help' for commands.\n";
    
    while (true) {
        std::cout << "vfs:" << cwd << "$ ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        std::string cmd, arg1, arg2;
        ss >> cmd >> arg1;
        
        if (cmd == "exit" || cmd == "quit") {
            break;
        } else if (cmd == "help") {
            std::cout << "Commands: ls [path], cd <path>, mkdir <path>, rmdir <path>, touch <path>, rm <path>, read <path>, write <path> <text>\n";
        } else if (cmd == "cd") {
            if (arg1.empty()) arg1 = "/";
            cwd = simplify_path(resolve_absolute_path(cwd, arg1));
            // In a more robust system, we would ask the server if the directory exists
        } else if (cmd == "ls") {
            std::string path = arg1.empty() ? cwd : simplify_path(resolve_absolute_path(cwd, arg1));
            send_command(fd, CMD_LS, path);
        } else if (cmd == "mkdir") {
            send_command(fd, CMD_MKDIR, simplify_path(resolve_absolute_path(cwd, arg1)));
        } else if (cmd == "rmdir") {
            send_command(fd, CMD_RMDIR, simplify_path(resolve_absolute_path(cwd, arg1)));
        } else if (cmd == "touch") {
            send_command(fd, CMD_TOUCH, simplify_path(resolve_absolute_path(cwd, arg1)));
        } else if (cmd == "rm") {
            send_command(fd, CMD_RM, simplify_path(resolve_absolute_path(cwd, arg1)));
        } else if (cmd == "read") {
            send_command(fd, CMD_READ, simplify_path(resolve_absolute_path(cwd, arg1)));
        } else if (cmd == "write") {
            std::string text;
            std::getline(ss, text);
            if (!text.empty() && text[0] == ' ') text = text.substr(1);
            std::vector<char> data(text.begin(), text.end());
            send_command(fd, CMD_WRITE, simplify_path(resolve_absolute_path(cwd, arg1)), data);
        } else {
            std::cout << "Unknown command.\n";
        }
    }
    
    close(fd);
    return 0;
}
