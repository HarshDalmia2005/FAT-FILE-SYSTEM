#pragma once
#include <cstdint>

#define SOCKET_PATH "/tmp/vfs_socket"

enum CommandType : uint32_t {
    CMD_MKDIR = 1,
    CMD_RMDIR,
    CMD_LS,
    CMD_TOUCH,
    CMD_RM,
    CMD_READ,
    CMD_WRITE
};

#pragma pack(push, 1)
struct RequestHeader {
    CommandType type;
    uint32_t path_len;
    uint32_t data_len;
};

struct ResponseHeader {
    uint32_t status; // 0 for success, 1 for error
    uint32_t data_len;
};
#pragma pack(pop)
