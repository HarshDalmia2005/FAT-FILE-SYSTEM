#pragma once

#include <string>
#include <cstdint>
#include <vector>

#define FAT_MAGIC 0xFA75A5E5
#define DEFAULT_BLOCK_SIZE 4096
#define DEFAULT_DISK_SIZE (1024ULL * 1024ULL * 1024ULL) // 1GB

#pragma pack(push, 1)
struct Superblock {
    uint32_t magic_number;
    uint32_t disk_size;          // in bytes (upper 32 bits if needed, but 1GB fits in 32-bit)
    uint32_t block_size;         // in bytes (4096)
    uint32_t total_blocks;       // 262144
    uint32_t fat_start_block;
    uint32_t fat_num_blocks;
    uint32_t journal_start_block;
    uint32_t journal_num_blocks;
    uint32_t root_dir_start_block;
};
#pragma pack(pop)

class Disk {
private:
    std::string disk_name;
    int fd;
    Superblock superblock;
    bool is_mounted;

public:
    Disk(const std::string& name);
    ~Disk();

    // Creates a new formatted sparse file with the superblock and FAT structures
    bool format();
    
    // Mounts an existing disk
    bool mount();
    
    // Unmounts the disk
    void unmount();
    
    // Low-level block I/O (thread-safe using pread/pwrite)
    bool read_block(uint32_t block_num, void* buffer);
    bool write_block(uint32_t block_num, const void* buffer);
    
    const Superblock& get_superblock() const { return superblock; }
    bool mounted() const { return is_mounted; }
};
