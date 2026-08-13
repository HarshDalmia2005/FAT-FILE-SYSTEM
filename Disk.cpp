#include "Disk.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <stdexcept>

Disk::Disk(const std::string& name) : disk_name(name), fd(-1), is_mounted(false) {
    memset(&superblock, 0, sizeof(Superblock));
}

Disk::~Disk() {
    unmount();
}

bool Disk::format() {
    if (is_mounted) {
        std::cerr << "Disk is already mounted. Cannot format." << std::endl;
        return false;
    }

    // Open file for read/write, create if it doesn't exist, truncate to zero if it does
    fd = open(disk_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open failed");
        return false;
    }

    // Use ftruncate to instantly create a 1GB sparse file
    if (ftruncate(fd, DEFAULT_DISK_SIZE) != 0) {
        perror("ftruncate failed");
        close(fd);
        fd = -1;
        return false;
    }

    // Initialize the superblock
    superblock.magic_number = FAT_MAGIC;
    superblock.disk_size = DEFAULT_DISK_SIZE;
    superblock.block_size = DEFAULT_BLOCK_SIZE;
    superblock.total_blocks = DEFAULT_DISK_SIZE / DEFAULT_BLOCK_SIZE;

    // Calculate layout
    // Block 0: Superblock
    // FAT needs to store an entry (4 bytes) for each block: total_blocks * 4 bytes
    uint32_t fat_size_bytes = superblock.total_blocks * sizeof(uint32_t);
    uint32_t fat_blocks = (fat_size_bytes + DEFAULT_BLOCK_SIZE - 1) / DEFAULT_BLOCK_SIZE;

    superblock.fat_start_block = 1;
    superblock.fat_num_blocks = fat_blocks;

    // Journal area: arbitrary size, e.g., 256 blocks (1MB)
    superblock.journal_start_block = superblock.fat_start_block + superblock.fat_num_blocks;
    superblock.journal_num_blocks = 256;

    // Root directory starts immediately after journal
    superblock.root_dir_start_block = superblock.journal_start_block + superblock.journal_num_blocks;

    // Write superblock to block 0
    // We create a zeroed buffer of block_size, copy the struct into it, and write it
    std::vector<char> buffer(DEFAULT_BLOCK_SIZE, 0);
    memcpy(buffer.data(), &superblock, sizeof(Superblock));

    if (pwrite(fd, buffer.data(), DEFAULT_BLOCK_SIZE, 0) != DEFAULT_BLOCK_SIZE) {
        perror("pwrite superblock failed");
        close(fd);
        fd = -1;
        return false;
    }

    // Ensure metadata is flushed to disk
    fsync(fd);
    close(fd);
    fd = -1;
    
    std::cout << "Disk '" << disk_name << "' formatted successfully." << std::endl;
    std::cout << "Total Blocks: " << superblock.total_blocks << std::endl;
    std::cout << "FAT Start: Block " << superblock.fat_start_block << " (" << superblock.fat_num_blocks << " blocks)" << std::endl;
    std::cout << "Journal Start: Block " << superblock.journal_start_block << " (" << superblock.journal_num_blocks << " blocks)" << std::endl;
    std::cout << "Root Dir Start: Block " << superblock.root_dir_start_block << std::endl;

    return true;
}

bool Disk::mount() {
    if (is_mounted) return true;

    fd = open(disk_name.c_str(), O_RDWR);
    if (fd < 0) {
        perror("open failed");
        return false;
    }

    // Read the superblock
    std::vector<char> buffer(DEFAULT_BLOCK_SIZE, 0);
    if (pread(fd, buffer.data(), DEFAULT_BLOCK_SIZE, 0) != DEFAULT_BLOCK_SIZE) {
        perror("pread superblock failed");
        close(fd);
        fd = -1;
        return false;
    }

    memcpy(&superblock, buffer.data(), sizeof(Superblock));

    if (superblock.magic_number != FAT_MAGIC) {
        std::cerr << "Invalid magic number! Not a valid FAT disk." << std::endl;
        close(fd);
        fd = -1;
        return false;
    }

    is_mounted = true;
    std::cout << "Disk '" << disk_name << "' mounted successfully." << std::endl;
    return true;
}

void Disk::unmount() {
    if (is_mounted && fd >= 0) {
        fsync(fd);
        close(fd);
        fd = -1;
        is_mounted = false;
        std::cout << "Disk '" << disk_name << "' unmounted." << std::endl;
    }
}

bool Disk::read_block(uint32_t block_num, void* buffer) {
    if (!is_mounted || fd < 0) return false;
    if (block_num >= superblock.total_blocks) return false;

    off_t offset = static_cast<off_t>(block_num) * superblock.block_size;
    ssize_t bytes_read = pread(fd, buffer, superblock.block_size, offset);
    
    return (bytes_read == superblock.block_size);
}

bool Disk::write_block(uint32_t block_num, const void* buffer) {
    if (!is_mounted || fd < 0) return false;
    if (block_num >= superblock.total_blocks) return false;

    off_t offset = static_cast<off_t>(block_num) * superblock.block_size;
    ssize_t bytes_written = pwrite(fd, buffer, superblock.block_size, offset);
    
    return (bytes_written == superblock.block_size);
}
