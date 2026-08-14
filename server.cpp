#include "Disk.h"
#include "FAT.h"
#include "VFS.h"
#include "Protocol.h"
#include <iostream>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

void handle_client(int client_fd, VFS& vfs, Disk& disk) {
    while (true) {
        RequestHeader req;
        ssize_t bytes_read = read(client_fd, &req, sizeof(req));
        if (bytes_read <= 0) break; // Client disconnected or error
        
        std::string path(req.path_len, '\0');
        if (req.path_len > 0) {
            read(client_fd, &path[0], req.path_len);
        }
        
        std::vector<char> data(req.data_len);
        if (req.data_len > 0) {
            read(client_fd, data.data(), req.data_len);
        }
        
        ResponseHeader res = {1, 0}; // default error
        std::vector<char> res_data;
        
        switch (req.type) {
            case CMD_MKDIR: {
                uint32_t out_block;
                size_t last_slash = path.find_last_of('/');
                if (last_slash != std::string::npos) {
                    std::string parent_path = path.substr(0, last_slash);
                    if (parent_path.empty()) parent_path = "/";
                    std::string dirname = path.substr(last_slash + 1);
                    
                    DirectoryEntry parent_entry;
                    if (vfs.resolve_path(parent_path, parent_entry)) {
                        if (vfs.create_entry(parent_entry.start_block, dirname, ATTR_DIR, out_block)) {
                            res.status = 0;
                        }
                    }
                }
                break;
            }
            case CMD_TOUCH: {
                uint32_t out_block;
                size_t last_slash = path.find_last_of('/');
                if (last_slash != std::string::npos) {
                    std::string parent_path = path.substr(0, last_slash);
                    if (parent_path.empty()) parent_path = "/";
                    std::string filename = path.substr(last_slash + 1);
                    
                    DirectoryEntry parent_entry;
                    if (vfs.resolve_path(parent_path, parent_entry)) {
                        if (vfs.create_entry(parent_entry.start_block, filename, ATTR_FILE, out_block)) {
                            res.status = 0;
                        }
                    }
                }
                break;
            }
            case CMD_RMDIR:
            case CMD_RM: {
                if (vfs.remove_entry(path)) {
                    res.status = 0;
                }
                break;
            }
            case CMD_LS: {
                DirectoryEntry entry;
                if (vfs.resolve_path(path, entry) && entry.attributes == ATTR_DIR) {
                    auto contents = vfs.list_directory(entry.start_block);
                    std::ostringstream oss;
                    for (const auto& item : contents) {
                        oss << (item.attributes == ATTR_DIR ? "d " : "- ") << item.filename << " " << item.size << "\n";
                    }
                    std::string out_str = oss.str();
                    res_data.assign(out_str.begin(), out_str.end());
                    res.data_len = res_data.size();
                    res.status = 0;
                }
                break;
            }
            case CMD_READ: {
                if (vfs.read_file(path, res_data)) {
                    res.data_len = res_data.size();
                    res.status = 0;
                }
                break;
            }
            case CMD_WRITE: {
                if (vfs.write_file(path, data)) {
                    res.status = 0;
                }
                break;
            }
        }
        
        write(client_fd, &res, sizeof(res));
        if (res.data_len > 0) {
            write(client_fd, res_data.data(), res.data_len);
        }
    }
    close(client_fd);
}

int main() {
    Disk disk("virtual_disk.bin");
    if (!disk.mount()) {
        std::cerr << "Failed to mount disk. Make sure to format it first!" << std::endl;
        return 1;
    }
    FAT fat(disk);
    if (!fat.mount()) return 1;
    VFS vfs(disk, fat);
    
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    unlink(SOCKET_PATH);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }
    
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        return 1;
    }
    
    std::cout << "VFS Server listening on " << SOCKET_PATH << "..." << std::endl;
    
    while (true) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd != -1) {
            std::cout << "Client connected." << std::endl;
            handle_client(client_fd, vfs, disk);
            std::cout << "Client disconnected." << std::endl;
        }
    }
    
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}
