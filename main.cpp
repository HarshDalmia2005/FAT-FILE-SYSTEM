#include "Disk.h"
#include "FAT.h"
#include <iostream>
#include <vector>
#include <cassert>

int main() {
    Disk disk("virtual_disk.bin");

    std::cout << "--- Testing Format ---" << std::endl;
    if (!disk.format()) {
        std::cerr << "Formatting failed!" << std::endl;
        return 1;
    }

    std::cout << "\n--- Testing Mount ---" << std::endl;
    if (!disk.mount()) {
        std::cerr << "Mounting failed!" << std::endl;
        return 1;
    }

    FAT fat(disk);
    std::cout << "\n--- Testing FAT Format ---" << std::endl;
    if (!fat.format()) {
        std::cerr << "FAT formatting failed!" << std::endl;
        return 1;
    }

    std::cout << "\n--- Testing FAT Mount ---" << std::endl;
    // Remount to test loading from disk
    fat.unmount();
    if (!fat.mount()) {
        std::cerr << "FAT mounting failed!" << std::endl;
        return 1;
    }

    std::cout << "\n--- Testing FAT Allocation ---" << std::endl;
    uint32_t b1 = fat.allocate_block();
    uint32_t b2 = fat.allocate_block();
    std::cout << "Allocated block " << b1 << std::endl;
    std::cout << "Allocated block " << b2 << std::endl;
    assert(b1 != 0 && b2 != 0 && b1 != b2);
    assert(fat.get_next(b1) == FAT::FAT_EOF);

    std::cout << "\n--- Testing FAT Chain & Free ---" << std::endl;
    fat.set_next(b1, b2);
    assert(fat.get_next(b1) == b2);
    
    if (fat.free_chain(b1)) {
        std::cout << "Freed chain starting at block " << b1 << std::endl;
    } else {
        std::cerr << "Failed to free chain!" << std::endl;
        return 1;
    }
    
    // Check if b1 and b2 are free
    assert(fat.get_next(b1) == FAT::FAT_FREE);
    assert(fat.get_next(b2) == FAT::FAT_FREE);

    std::cout << "\nAll FAT tests passed!" << std::endl;

    fat.unmount();
    disk.unmount();
    return 0;
}
