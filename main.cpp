#include "Disk.h"
#include "FAT.h"
#include "VFS.h"
#include <iostream>
#include <vector>
#include <cassert>

int main() {
    Disk disk("virtual_disk.bin");

    std::cout << "--- Testing Format ---" << std::endl;
    if (!disk.format()) return 1;
    if (!disk.mount()) return 1;
    std::cout << "Disk 'virtual_disk.bin' mounted successfully.\n";

    Journal journal(disk);
    journal.replay();

    FAT fat(disk, &journal);
    std::cout << "Formatting FAT... ";
    if (!fat.format()) return 1;
    
    VFS vfs(disk, fat);
    if (!vfs.format()) return 1;

    std::cout << "\n--- Testing VFS ---" << std::endl;
    uint32_t folder_block;
    if (vfs.create_entry(disk.get_superblock().root_dir_start_block, "docs", ATTR_DIR, folder_block)) {
        std::cout << "Created directory 'docs'." << std::endl;
    } else {
        std::cerr << "Failed to create directory." << std::endl;
        return 1;
    }

    uint32_t file_block;
    if (vfs.create_entry(folder_block, "hello.txt", ATTR_FILE, file_block)) {
        std::cout << "Created file 'hello.txt' inside 'docs'." << std::endl;
    } else {
        std::cerr << "Failed to create file." << std::endl;
        return 1;
    }

    std::cout << "\n--- Testing Path Resolution ---" << std::endl;
    DirectoryEntry entry;
    if (vfs.resolve_path("/docs/hello.txt", entry)) {
        std::cout << "Resolved '/docs/hello.txt'. Start Block: " << entry.start_block << std::endl;
    } else {
        std::cerr << "Failed to resolve path." << std::endl;
        return 1;
    }

    std::cout << "\n--- Testing Directory Listing ---" << std::endl;
    auto root_contents = vfs.list_directory(disk.get_superblock().root_dir_start_block);
    std::cout << "Root contains " << root_contents.size() << " entry." << std::endl;
    
    auto docs_contents = vfs.list_directory(folder_block);
    std::cout << "'docs' contains " << docs_contents.size() << " entry. Name: " << docs_contents[0].filename << std::endl;

    std::cout << "\nAll VFS tests passed!" << std::endl;

    fat.unmount();
    disk.unmount();
    return 0;
}
