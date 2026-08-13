# Custom FAT-Based Virtual File System

This project is a disk-backed FAT (File Allocation Table) virtual file system implemented in C++. It utilizes POSIX APIs for threading, synchronization, and binary I/O to simulate a persistent storage environment. The architecture follows a multi-phase approach, beginning with fundamental disk abstraction and culminating in a concurrent client-server model with metadata journaling.

## Phase 1: Core Disk Abstraction & Sparse File Initialization

In this initial phase, the foundational block-level I/O mechanisms are established. The virtual disk acts as the raw storage medium, simulating a physical hard drive by treating a large file on the host OS as a block device.

### 1. The Virtual Disk
Instead of allocating the entire 1GB disk size upfront, which is slow and consumes unnecessary physical storage, we use **POSIX sparse files**. By leveraging `ftruncate()`, the operating system immediately registers the file as being 1GB in size, but physical disk blocks are only allocated on-demand as data is actually written. This ensures instant formatting while remaining highly efficient.

### 2. Thread-Safe Binary I/O
The `Disk` class abstracts the complexities of the underlying file descriptor.
- We utilize `pread()` and `pwrite()` instead of standard C++ streams (`std::fstream`) or `read()`/`write()`.
- Unlike `read()`, `pread()` takes an explicit offset argument and **does not modify the file descriptor's internal offset pointer**. This design guarantees thread safety, allowing multiple threads to read from or write to different blocks simultaneously without requiring a mutex lock on the file descriptor itself.

### 3. Disk Layout & Superblock
The virtual disk is divided into fixed-size blocks (4KB by default). The first block (Block 0) is reserved for the **Superblock**. The Superblock contains critical metadata about the file system's geometry:

*   **Magic Number**: A unique identifier (`0xFA75A5E5`) to confirm the file is a valid disk formatted for this specific system.
*   **Disk & Block Size**: Essential parameters (e.g., 1GB disk size, 4096 bytes per block, resulting in 262,144 total blocks).
*   **FAT Location**: The starting block and size of the File Allocation Table (e.g., starting at Block 1, consuming 256 blocks).
*   **Journal Area Location**: The starting block and size reserved for the circular intent log (crash recovery).
*   **Root Directory**: The block index where the root directory entries begin.

### How to Compile and Test Phase 1
A simple test driver is provided to verify the formatting, mounting, and low-level block I/O.

```bash
# Compile the project
make

# Run the test executable
./vfs_test
```

Expected output:
```
--- Testing Format ---
Disk 'virtual_disk.bin' formatted successfully.
Total Blocks: 262144
FAT Start: Block 1 (256 blocks)
Journal Start: Block 257 (256 blocks)
Root Dir Start: Block 513

--- Testing Mount ---
Disk 'virtual_disk.bin' mounted successfully.

--- Testing Read/Write ---
Read/Write verification passed for block 500.
Disk 'virtual_disk.bin' unmounted.
```

*Upcoming in Phase 2: FAT Implementation & Block Allocation.*
