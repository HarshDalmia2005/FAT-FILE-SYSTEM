#include "Disk.h"
#include <iostream>
#include <vector>

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

    std::cout << "\n--- Testing Read/Write ---" << std::endl;
    uint32_t block_to_test = 500;
    std::vector<char> write_data(disk.get_superblock().block_size, 'A');
    if (!disk.write_block(block_to_test, write_data.data())) {
        std::cerr << "Write block failed!" << std::endl;
        return 1;
    }

    std::vector<char> read_data(disk.get_superblock().block_size, 0);
    if (!disk.read_block(block_to_test, read_data.data())) {
        std::cerr << "Read block failed!" << std::endl;
        return 1;
    }

    bool match = true;
    for (size_t i = 0; i < disk.get_superblock().block_size; ++i) {
        if (read_data[i] != write_data[i]) {
            match = false;
            break;
        }
    }

    if (match) {
        std::cout << "Read/Write verification passed for block " << block_to_test << "." << std::endl;
    } else {
        std::cerr << "Data mismatch!" << std::endl;
    }

    disk.unmount();
    return 0;
}
