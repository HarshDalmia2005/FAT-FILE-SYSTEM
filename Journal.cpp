#include "Journal.h"
#include <iostream>
#include <cstring>

Journal::Journal(Disk& d) : disk(d), current_idx(0) {
    if (disk.mounted()) {
        const Superblock& sb = disk.get_superblock();
        start_block = sb.journal_start_block;
        num_blocks = sb.journal_num_blocks;
    }
}

uint32_t Journal::find_next_slot() {
    uint32_t max_tx = num_blocks / 2;
    uint32_t slot = current_idx;
    current_idx = (current_idx + 1) % max_tx;
    return slot;
}

int Journal::log_transaction(uint32_t target_block, const void* data) {
    if (!disk.mounted()) return -1;
    
    uint32_t tx_id = find_next_slot();
    uint32_t header_block = start_block + (tx_id * 2);
    uint32_t data_block = header_block + 1;
    
    if (!disk.write_block(data_block, data)) return -1;
    
    std::vector<char> buffer(disk.get_superblock().block_size, 0);
    JournalHeader* header = reinterpret_cast<JournalHeader*>(buffer.data());
    header->magic = JOURNAL_MAGIC;
    header->state = STATE_PENDING;
    header->target_block = target_block;
    
    if (!disk.write_block(header_block, buffer.data())) return -1;
    
    return tx_id;
}

bool Journal::commit_transaction(int tx_id) {
    if (tx_id < 0 || !disk.mounted()) return false;
    
    uint32_t header_block = start_block + (tx_id * 2);
    
    std::vector<char> buffer(disk.get_superblock().block_size, 0);
    if (!disk.read_block(header_block, buffer.data())) return false;
    
    JournalHeader* header = reinterpret_cast<JournalHeader*>(buffer.data());
    if (header->magic == JOURNAL_MAGIC && header->state == STATE_PENDING) {
        header->state = STATE_COMMITTED;
        return disk.write_block(header_block, buffer.data());
    }
    
    return false;
}

bool Journal::safe_write_block(uint32_t target_block, const void* data) {
    int tx_id = log_transaction(target_block, data);
    if (tx_id == -1) return false; // Fallback to unsafe if journal fails? No, fail.
    
    if (!disk.write_block(target_block, data)) return false;
    
    return commit_transaction(tx_id);
}

bool Journal::replay() {
    if (!disk.mounted()) return false;
    
    uint32_t max_tx = num_blocks / 2;
    std::vector<char> header_buf(disk.get_superblock().block_size);
    std::vector<char> data_buf(disk.get_superblock().block_size);
    
    int replayed_count = 0;
    
    for (uint32_t i = 0; i < max_tx; ++i) {
        uint32_t header_block = start_block + (i * 2);
        if (!disk.read_block(header_block, header_buf.data())) continue;
        
        JournalHeader* header = reinterpret_cast<JournalHeader*>(header_buf.data());
        
        if (header->magic == JOURNAL_MAGIC && header->state == STATE_PENDING) {
            std::cout << "Recovering pending transaction for block " << header->target_block << "..." << std::endl;
            
            uint32_t data_block = header_block + 1;
            if (disk.read_block(data_block, data_buf.data())) {
                disk.write_block(header->target_block, data_buf.data());
                
                header->state = STATE_COMMITTED;
                disk.write_block(header_block, header_buf.data());
                replayed_count++;
            }
        }
    }
    
    if (replayed_count > 0) {
        std::cout << "Journal replay complete. Recovered " << replayed_count << " blocks." << std::endl;
    }
    
    return true;
}
