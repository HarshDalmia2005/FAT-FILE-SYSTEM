#pragma once
#include "Disk.h"
#include <cstdint>
#include <vector>

#define JOURNAL_MAGIC 0x4A4F5552
#define STATE_FREE 0
#define STATE_PENDING 1
#define STATE_COMMITTED 2

#pragma pack(push, 1)
struct JournalHeader {
    uint32_t magic;
    uint32_t state;
    uint32_t target_block;
    uint8_t padding[4096 - 12];
};
#pragma pack(pop)

class Journal {
private:
    Disk& disk;
    uint32_t start_block;
    uint32_t num_blocks;
    uint32_t current_idx;
    
public:
    Journal(Disk& d);
    
    uint32_t find_next_slot();
    int log_transaction(uint32_t target_block, const void* data);
    bool commit_transaction(int tx_id);
    bool replay();
    
    // Replaces disk.write_block for metadata operations
    bool safe_write_block(uint32_t target_block, const void* data);
};
