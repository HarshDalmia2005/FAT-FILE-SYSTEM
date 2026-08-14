#include "VFS.h"
#include <iostream>
#include <cstring>
#include <sstream>

VFS::VFS(Disk& d, FAT& f) : disk(d), fat(f) {}

std::vector<std::string> VFS::split_path(const std::string& path) const {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(path);
    while (std::getline(tokenStream, token, '/')) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

bool VFS::format() {
    const Superblock& sb = disk.get_superblock();
    
    // The root directory starts at sb.root_dir_start_block.
    // It's already marked as EOF in FAT::format().
    // We just need to initialize the first block of the root directory with zeros (all entries UNUSED).
    
    std::vector<char> zeros(sb.block_size, 0);
    if (!disk.write_block(sb.root_dir_start_block, zeros.data())) {
        std::cerr << "Failed to initialize root directory block." << std::endl;
        return false;
    }
    
    std::cout << "VFS root directory formatted." << std::endl;
    return true;
}

bool VFS::find_in_directory(uint32_t dir_start_block, const std::string& filename, DirectoryEntry& out_entry, uint32_t& out_index) {
    uint32_t current_block = dir_start_block;
    const Superblock& sb = disk.get_superblock();
    uint32_t entries_per_block = sb.block_size / sizeof(DirectoryEntry);
    
    std::vector<char> buffer(sb.block_size);
    uint32_t global_index = 0;
    
    while (current_block != FAT::FAT_EOF && current_block != FAT::FAT_FREE) {
        if (!disk.read_block(current_block, buffer.data())) {
            return false;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer.data());
        for (uint32_t i = 0; i < entries_per_block; ++i) {
            if (entries[i].attributes != ATTR_UNUSED && strncmp(entries[i].filename, filename.c_str(), 244) == 0) {
                out_entry = entries[i];
                out_index = global_index;
                return true;
            }
            global_index++;
        }
        
        current_block = fat.get_next(current_block);
    }
    
    return false;
}

bool VFS::create_entry(uint32_t parent_dir_block, const std::string& filename, uint8_t attributes, uint32_t& out_start_block) {
    if (filename.length() >= 244) {
        std::cerr << "Filename too long." << std::endl;
        return false;
    }
    
    DirectoryEntry existing;
    uint32_t idx;
    if (find_in_directory(parent_dir_block, filename, existing, idx)) {
        std::cerr << "File/Directory already exists." << std::endl;
        return false;
    }
    
    uint32_t current_block = parent_dir_block;
    uint32_t last_block = parent_dir_block;
    const Superblock& sb = disk.get_superblock();
    uint32_t entries_per_block = sb.block_size / sizeof(DirectoryEntry);
    
    std::vector<char> buffer(sb.block_size);
    
    // Find an unused entry
    while (current_block != FAT::FAT_EOF && current_block != FAT::FAT_FREE) {
        if (!disk.read_block(current_block, buffer.data())) return false;
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer.data());
        for (uint32_t i = 0; i < entries_per_block; ++i) {
            if (entries[i].attributes == ATTR_UNUSED) {
                // Found a slot!
                out_start_block = fat.allocate_block();
                if (out_start_block == 0) return false; // Disk full
                
                // If it's a directory, initialize it with 0s
                if (attributes == ATTR_DIR) {
                    std::vector<char> zeros(sb.block_size, 0);
                    disk.write_block(out_start_block, zeros.data());
                }
                
                strncpy(entries[i].filename, filename.c_str(), 244);
                entries[i].start_block = out_start_block;
                entries[i].size = 0;
                entries[i].attributes = attributes;
                
                return disk.write_block(current_block, buffer.data());
            }
        }
        last_block = current_block;
        current_block = fat.get_next(current_block);
    }
    
    // If we reach here, we need to extend the directory's block chain
    uint32_t new_dir_block = fat.allocate_block();
    if (new_dir_block == 0) return false;
    
    fat.set_next(last_block, new_dir_block);
    
    std::vector<char> zeros(sb.block_size, 0);
    DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(zeros.data());
    
    out_start_block = fat.allocate_block();
    if (out_start_block == 0) return false; // Disk full
    
    if (attributes == ATTR_DIR) {
        std::vector<char> dir_zeros(sb.block_size, 0);
        disk.write_block(out_start_block, dir_zeros.data());
    }
    
    strncpy(entries[0].filename, filename.c_str(), 244);
    entries[0].start_block = out_start_block;
    entries[0].size = 0;
    entries[0].attributes = attributes;
    
    return disk.write_block(new_dir_block, zeros.data());
}

bool VFS::resolve_path(const std::string& path, DirectoryEntry& out_entry) {
    std::vector<std::string> parts = split_path(path);
    const Superblock& sb = disk.get_superblock();
    
    uint32_t current_dir_block = sb.root_dir_start_block;
    
    if (parts.empty()) {
        // Return root directory
        out_entry.attributes = ATTR_DIR;
        out_entry.start_block = current_dir_block;
        strcpy(out_entry.filename, "/");
        return true;
    }
    
    DirectoryEntry current_entry;
    for (size_t i = 0; i < parts.size(); ++i) {
        uint32_t index;
        if (!find_in_directory(current_dir_block, parts[i], current_entry, index)) {
            return false; // Not found
        }
        
        if (i < parts.size() - 1 && current_entry.attributes != ATTR_DIR) {
            return false; // Intermediate path component is not a directory
        }
        
        current_dir_block = current_entry.start_block;
    }
    
    out_entry = current_entry;
    return true;
}

std::vector<DirectoryEntry> VFS::list_directory(uint32_t dir_start_block) {
    std::vector<DirectoryEntry> results;
    uint32_t current_block = dir_start_block;
    const Superblock& sb = disk.get_superblock();
    uint32_t entries_per_block = sb.block_size / sizeof(DirectoryEntry);
    
    std::vector<char> buffer(sb.block_size);
    
    while (current_block != FAT::FAT_EOF && current_block != FAT::FAT_FREE) {
        if (!disk.read_block(current_block, buffer.data())) {
            break;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer.data());
        for (uint32_t i = 0; i < entries_per_block; ++i) {
            if (entries[i].attributes != ATTR_UNUSED) {
                results.push_back(entries[i]);
            }
        }
        
        current_block = fat.get_next(current_block);
    }
    
    return results;
}

bool VFS::resolve_path_full(const std::string& path, DirectoryEntry& out_entry, uint32_t& out_parent_block, uint32_t& out_index) {
    std::vector<std::string> parts = split_path(path);
    const Superblock& sb = disk.get_superblock();
    
    uint32_t current_dir_block = sb.root_dir_start_block;
    
    if (parts.empty()) {
        out_entry.attributes = ATTR_DIR;
        out_entry.start_block = current_dir_block;
        strcpy(out_entry.filename, "/");
        out_parent_block = 0;
        out_index = 0;
        return true;
    }
    
    DirectoryEntry current_entry;
    uint32_t current_index;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (!find_in_directory(current_dir_block, parts[i], current_entry, current_index)) {
            return false;
        }
        
        if (i < parts.size() - 1) {
            if (current_entry.attributes != ATTR_DIR) {
                return false;
            }
            current_dir_block = current_entry.start_block;
        } else {
            out_parent_block = current_dir_block;
            out_index = current_index;
            out_entry = current_entry;
        }
    }
    return true;
}

bool VFS::remove_entry(const std::string& path) {
    DirectoryEntry entry;
    uint32_t parent_block;
    uint32_t index;
    if (!resolve_path_full(path, entry, parent_block, index)) return false;
    if (path == "/" || path == "") return false;
    
    const Superblock& sb = disk.get_superblock();
    uint32_t entries_per_block = sb.block_size / sizeof(DirectoryEntry);
    
    uint32_t current_block = parent_block;
    uint32_t block_index = index / entries_per_block;
    uint32_t entry_offset = index % entries_per_block;
    
    for (uint32_t i = 0; i < block_index; ++i) {
        current_block = fat.get_next(current_block);
        if (current_block == FAT::FAT_EOF || current_block == FAT::FAT_FREE) return false;
    }
    
    if (entry.attributes == ATTR_DIR) {
        auto contents = list_directory(entry.start_block);
        if (!contents.empty()) {
            std::cerr << "Directory not empty" << std::endl;
            return false;
        }
    }
    
    if (entry.start_block != 0) {
        fat.free_chain(entry.start_block);
    }
    
    std::vector<char> buffer(sb.block_size);
    if (!disk.read_block(current_block, buffer.data())) return false;
    
    DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer.data());
    entries[entry_offset].attributes = ATTR_UNUSED;
    
    return disk.write_block(current_block, buffer.data());
}

bool VFS::read_file(const std::string& path, std::vector<char>& out_data) {
    DirectoryEntry entry;
    if (!resolve_path(path, entry)) return false;
    if (entry.attributes != ATTR_FILE) return false;
    
    out_data.resize(entry.size);
    if (entry.size == 0) return true;
    
    const Superblock& sb = disk.get_superblock();
    uint32_t current_block = entry.start_block;
    uint32_t bytes_read = 0;
    
    std::vector<char> buffer(sb.block_size);
    while (current_block != FAT::FAT_EOF && current_block != FAT::FAT_FREE && bytes_read < entry.size) {
        if (!disk.read_block(current_block, buffer.data())) return false;
        
        uint32_t bytes_to_copy = std::min((uint32_t)sb.block_size, entry.size - bytes_read);
        std::memcpy(out_data.data() + bytes_read, buffer.data(), bytes_to_copy);
        bytes_read += bytes_to_copy;
        
        current_block = fat.get_next(current_block);
    }
    
    return true;
}

bool VFS::write_file(const std::string& path, const std::vector<char>& data) {
    DirectoryEntry entry;
    uint32_t parent_block;
    uint32_t index;
    if (!resolve_path_full(path, entry, parent_block, index)) return false;
    if (entry.attributes != ATTR_FILE) return false;
    
    const Superblock& sb = disk.get_superblock();
    
    if (entry.start_block != 0) {
        fat.free_chain(entry.start_block);
    }
    
    uint32_t new_start_block = 0;
    if (!data.empty()) {
        uint32_t current_block = 0;
        uint32_t previous_block = 0;
        uint32_t bytes_written = 0;
        
        while (bytes_written < data.size()) {
            current_block = fat.allocate_block();
            if (current_block == 0) return false;
            
            if (previous_block == 0) {
                new_start_block = current_block;
            } else {
                fat.set_next(previous_block, current_block);
            }
            
            std::vector<char> buffer(sb.block_size, 0);
            uint32_t bytes_to_copy = std::min((uint32_t)sb.block_size, (uint32_t)data.size() - bytes_written);
            std::memcpy(buffer.data(), data.data() + bytes_written, bytes_to_copy);
            
            if (!disk.write_block(current_block, buffer.data())) return false;
            
            bytes_written += bytes_to_copy;
            previous_block = current_block;
        }
    } else {
        new_start_block = fat.allocate_block();
        if (new_start_block == 0) return false;
        std::vector<char> buffer(sb.block_size, 0);
        disk.write_block(new_start_block, buffer.data());
    }
    
    uint32_t entries_per_block = sb.block_size / sizeof(DirectoryEntry);
    uint32_t dir_block = parent_block;
    uint32_t block_index = index / entries_per_block;
    uint32_t entry_offset = index % entries_per_block;
    
    for (uint32_t i = 0; i < block_index; ++i) {
        dir_block = fat.get_next(dir_block);
    }
    
    std::vector<char> buffer(sb.block_size);
    if (!disk.read_block(dir_block, buffer.data())) return false;
    
    DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer.data());
    entries[entry_offset].start_block = new_start_block;
    entries[entry_offset].size = data.size();
    
    return disk.write_block(dir_block, buffer.data());
}
