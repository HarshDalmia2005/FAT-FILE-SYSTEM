#pragma once

#include "Disk.h"
#include "FAT.h"
#include <string>
#include <vector>

#define ATTR_UNUSED 0x00
#define ATTR_FILE   0x01
#define ATTR_DIR    0x02

#pragma pack(push, 1)
struct DirectoryEntry {
    char filename[244];
    uint32_t start_block;
    uint32_t size;
    uint8_t attributes;
    uint8_t reserved[3];
};
#pragma pack(pop)

class VFS {
private:
    Disk& disk;
    FAT& fat;

    // Helper to split a path by '/'
    std::vector<std::string> split_path(const std::string& path) const;

public:
    VFS(Disk& d, FAT& f);

    // Initializes the root directory on a freshly formatted disk
    bool format();

    // Finds a file or directory within a specific directory block chain
    // Returns true if found, and fills out_entry and out_index (the index in the directory)
    bool find_in_directory(uint32_t dir_start_block, const std::string& filename, DirectoryEntry& out_entry, uint32_t& out_index);

    // Creates a new entry (file or directory) inside a parent directory
    bool create_entry(uint32_t parent_dir_block, const std::string& filename, uint8_t attributes, uint32_t& out_start_block);

    // Resolves an absolute path (e.g., "/folder/file.txt") to a DirectoryEntry
    bool resolve_path(const std::string& path, DirectoryEntry& out_entry);

    // Reads all entries in a directory (for ls)
    std::vector<DirectoryEntry> list_directory(uint32_t dir_start_block);

    // Resolves path and gets parent info for modification
    bool resolve_path_full(const std::string& path, DirectoryEntry& out_entry, uint32_t& out_parent_block, uint32_t& out_index);

    // Removes a file or an empty directory
    bool remove_entry(const std::string& path);

    // Reads entire file content
    bool read_file(const std::string& path, std::vector<char>& out_data);

    // Writes/overwrites file content
    bool write_file(const std::string& path, const std::vector<char>& data);
};
