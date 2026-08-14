#include "Protocol.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

// ANSI Color Codes
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define BRIGHT_GREEN  "\033[92m"
#define BRIGHT_BLUE   "\033[94m"
#define BRIGHT_CYAN   "\033[96m"
#define BRIGHT_WHITE  "\033[97m"

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

void print_ls_output(const std::string& raw) {
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        // Format: "d dirname size" or "- filename size"
        char type = line[0];
        std::istringstream lss(line.substr(2));
        std::string name;
        uint32_t size;
        lss >> name >> size;

        if (type == 'd') {
            std::cout << BRIGHT_BLUE << BOLD << "d " << RESET
                      << BRIGHT_BLUE << BOLD << name << RESET
                      << DIM << "  <DIR>" << RESET << "\n";
        } else {
            std::cout << GREEN << "- " << RESET
                      << WHITE << name << RESET
                      << DIM << "  " << size << " bytes" << RESET << "\n";
        }
    }
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
            std::string output(res_data.data());

            if (type == CMD_LS) {
                print_ls_output(output);
            } else if (type == CMD_READ) {
                std::cout << BRIGHT_WHITE << output << RESET;
                if (output.empty() || output.back() != '\n') std::cout << "\n";
            } else {
                std::cout << output;
            }
        } else {
            std::cout << BRIGHT_GREEN << "✓ " << RESET << "Done.\n";
        }
    } else {
        std::cerr << RED << BOLD << "✗ Error: " << RESET << RED << "operation failed.\n" << RESET;
    }
}

int main() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << RED << BOLD << "✗ Failed to connect to VFS server." << RESET
                  << "\n  Make sure " << CYAN << "./vfs_server" << RESET << " is running.\n";
        return 1;
    }

    std::string cwd = "/";
    std::string line;

    // Banner
    std::cout << "\n"
              << BOLD << CYAN << "  ╔══════════════════════════════╗\n" << RESET
              << BOLD << CYAN << "  ║   " << RESET
              << BOLD << BRIGHT_WHITE << "FAT Virtual File System" << RESET
              << BOLD << CYAN << "   ║\n" << RESET
              << BOLD << CYAN << "  ╚══════════════════════════════╝\n" << RESET
              << DIM  << "  Type 'help' for available commands.\n\n" << RESET;

    while (true) {
        // Prompt: user@vfs:/path$
        std::cout << BOLD << GREEN  << "vfs" << RESET
                  << DIM           << "@" << RESET
                  << BOLD << CYAN  << "fs" << RESET
                  << DIM           << ":" << RESET
                  << BOLD << BRIGHT_BLUE << cwd << RESET
                  << BOLD << WHITE << "$ " << RESET;
        std::cout.flush();

        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cmd, arg1;
        ss >> cmd >> arg1;

        if (cmd == "exit" || cmd == "quit") {
            std::cout << DIM << "Goodbye.\n" << RESET;
            break;
        } else if (cmd == "help") {
            std::cout << "\n"
                      << BOLD << YELLOW << "Available commands:\n" << RESET
                      << "  " << CYAN << "ls" << RESET << " [path]          List directory contents\n"
                      << "  " << CYAN << "cd" << RESET << " <path>          Change directory\n"
                      << "  " << CYAN << "mkdir" << RESET << " <path>       Create a directory\n"
                      << "  " << CYAN << "rmdir" << RESET << " <path>       Remove an empty directory\n"
                      << "  " << CYAN << "touch" << RESET << " <path>       Create an empty file\n"
                      << "  " << CYAN << "rm" << RESET << " <path>          Remove a file\n"
                      << "  " << CYAN << "read" << RESET << " <path>        Print file contents\n"
                      << "  " << CYAN << "write" << RESET << " <path> <text> Write text to a file\n"
                      << "  " << CYAN << "exit" << RESET << "               Disconnect\n\n";
        } else if (cmd == "cd") {
            if (arg1.empty()) arg1 = "/";
            cwd = simplify_path(resolve_absolute_path(cwd, arg1));
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
            std::cerr << YELLOW << "Unknown command: " << RESET << "'" << cmd << "'. "
                      << DIM << "Type 'help' for usage.\n" << RESET;
        }
    }

    close(fd);
    return 0;
}
