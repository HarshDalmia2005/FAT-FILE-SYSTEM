#include "FAT.h"
#include <iostream>
#include <cstring>
#include <algorithm>

FAT::FAT(Disk& d) : disk(d) {
}

bool FAT::format() {
    if (!disk.mounted()) {
        std::cerr << "Cannot format FAT: disk is not mounted." << std::endl;
        return false;
    }

    const Superblock& sb = disk.get_superblock();
    
    // Resize table to hold all block entries, initialize all to FAT_FREE
    table.assign(sb.total_blocks, FAT_FREE);
    
    // Mark system blocks as reserved/EOF.
    // 1. Superblock (block 0)
    table[0] = FAT_EOF;
    
    // 2. FAT blocks
    for (uint32_t i = 0; i < sb.fat_num_blocks; ++i) {
        table[sb.fat_start_block + i] = FAT_EOF;
    }
    
    // 3. Journal blocks
    for (uint32_t i = 0; i < sb.journal_num_blocks; ++i) {
        table[sb.journal_start_block + i] = FAT_EOF;
    }
    
    // 4. Root directory block (assuming it takes 1 block initially)
    table[sb.root_dir_start_block] = FAT_EOF;
    
    // Flush the entire FAT to disk
    std::cout << "Formatting FAT... writing to disk." << std::endl;
    for (uint32_t i = 0; i < sb.fat_num_blocks; ++i) {
        if (!flush_block(i)) {
            std::cerr << "Failed to write FAT block " << i << std::endl;
            return false;
        }
    }
    
    return true;
}

bool FAT::mount() {
    if (!disk.mounted()) {
        std::cerr << "Cannot mount FAT: disk is not mounted." << std::endl;
        return false;
    }
    
    const Superblock& sb = disk.get_superblock();
    table.resize(sb.total_blocks);
    
    // Calculate how many FAT entries fit in one block
    uint32_t entries_per_block = sb.block_size / sizeof(uint32_t);
    std::vector<uint32_t> buffer(entries_per_block);
    
    for (uint32_t i = 0; i < sb.fat_num_blocks; ++i) {
        if (!disk.read_block(sb.fat_start_block + i, buffer.data())) {
            std::cerr << "Failed to read FAT block " << i << std::endl;
            return false;
        }
        
        uint32_t entries_to_copy = std::min(entries_per_block, sb.total_blocks - (i * entries_per_block));
        std::memcpy(&table[i * entries_per_block], buffer.data(), entries_to_copy * sizeof(uint32_t));
    }
    
    std::cout << "FAT loaded successfully. Free blocks: " << get_free_blocks_count() << std::endl;
    return true;
}

bool FAT::unmount() {
    table.clear();
    return true;
}

bool FAT::flush_block(uint32_t fat_block_index) {
    const Superblock& sb = disk.get_superblock();
    uint32_t entries_per_block = sb.block_size / sizeof(uint32_t);
    
    uint32_t start_entry = fat_block_index * entries_per_block;
    uint32_t entries_to_copy = std::min(entries_per_block, sb.total_blocks - start_entry);
    
    std::vector<uint32_t> buffer(entries_per_block, 0);
    std::memcpy(buffer.data(), &table[start_entry], entries_to_copy * sizeof(uint32_t));
    
    return disk.write_block(sb.fat_start_block + fat_block_index, buffer.data());
}

uint32_t FAT::allocate_block() {
    for (uint32_t i = 1; i < table.size(); ++i) {
        if (table[i] == FAT_FREE) {
            table[i] = FAT_EOF;
            
            const Superblock& sb = disk.get_superblock();
            uint32_t entries_per_block = sb.block_size / sizeof(uint32_t);
            uint32_t fat_block_index = i / entries_per_block;
            
            if (!flush_block(fat_block_index)) {
                table[i] = FAT_FREE; 
                return 0;
            }
            return i;
        }
    }
    return 0;
}

bool FAT::free_chain(uint32_t start_block) {
    uint32_t current = start_block;
    const Superblock& sb = disk.get_superblock();
    uint32_t entries_per_block = sb.block_size / sizeof(uint32_t);
    
    std::vector<bool> dirty_blocks(sb.fat_num_blocks, false);
    
    while (current != FAT_EOF && current != FAT_FREE && current < table.size()) {
        uint32_t next = table[current];
        table[current] = FAT_FREE;
        
        dirty_blocks[current / entries_per_block] = true;
        current = next;
    }
    
    bool success = true;
    for (uint32_t i = 0; i < sb.fat_num_blocks; ++i) {
        if (dirty_blocks[i]) {
            if (!flush_block(i)) success = false;
        }
    }
    
    return success;
}

uint32_t FAT::get_next(uint32_t block) const {
    if (block < table.size()) {
        return table[block];
    }
    return FAT_EOF;
}

bool FAT::set_next(uint32_t block, uint32_t next_block) {
    if (block >= table.size()) return false;
    
    table[block] = next_block;
    
    const Superblock& sb = disk.get_superblock();
    uint32_t entries_per_block = sb.block_size / sizeof(uint32_t);
    return flush_block(block / entries_per_block);
}

uint32_t FAT::get_free_blocks_count() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < table.size(); ++i) {
        if (table[i] == FAT_FREE) count++;
    }
    return count;
}
