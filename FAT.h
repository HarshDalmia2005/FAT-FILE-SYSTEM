#pragma once

#include "Disk.h"
#include <vector>
#include <cstdint>

#include "Journal.h"

class FAT {
private:
    Disk& disk;
    Journal* journal;
    std::vector<uint32_t> table;

    // Writes a single FAT block from memory to the virtual disk
    bool flush_block(uint32_t fat_block_index);

public:
    static const uint32_t FAT_FREE = 0x00000000;
    static const uint32_t FAT_EOF = 0xFFFFFFFF;
    static const uint32_t FAT_BAD = 0xFFFFFFF7;

    FAT(Disk& d, Journal* j = nullptr);
    
    // Initializes the FAT array for a newly formatted disk 
    // Marks system blocks (Superblock, FAT itself, Journal, Root Dir) as EOF/Reserved
    bool format();
    
    // Loads the FAT array from a mounted disk
    bool mount();
    
    // Flushes the entire FAT to disk (can be called during unmount)
    bool unmount();

    // Allocates a single free block, marks it as EOF, and returns its block number.
    // Returns 0 if no free blocks are available (block 0 is superblock, so 0 is an error value here).
    uint32_t allocate_block();
    
    // Frees a chain of blocks starting from `start_block`.
    bool free_chain(uint32_t start_block);
    
    // Gets the next block in the chain
    uint32_t get_next(uint32_t block) const;
    
    // Sets the next block in the chain and flushes the change to disk
    bool set_next(uint32_t block, uint32_t next_block);
    
    // Returns the total number of free blocks
    uint32_t get_free_blocks_count() const;
};
