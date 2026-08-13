# Custom FAT-Based Virtual File System

This plan outlines the architecture and phased implementation for building a disk-backed FAT Virtual File System in C++. The project is broken down into modular phases so you can commit your progress over several days/weeks, exactly as it would look for a realistic course project.

## User Review Required

> [!IMPORTANT]
> Please review the updated phased breakdown below. We have incorporated the best practices for the project (Client-Server architecture and Sparse Files). Once you approve, we will start executing Phase 1. 

## Proposed Architecture

To make this project stand out on your resume and accurately simulate a real operating system environment, we will use a **Client-Server architecture**. A central "File System Server" process will manage the virtual disk and use thread pools and readers-writer locks to handle concurrent requests from multiple "Client" processes.

To manage the 1GB disk efficiently, we will use **POSIX sparse files** (`ftruncate`). This makes formatting instantaneous and only consumes physical disk space as you actually write data to the virtual blocks.

### Layout of the Virtual Disk
1. **Superblock**: Contains disk size, block size (e.g., 4KB), FAT offset, Root Directory offset, and Journal offset.
2. **Journal Area**: A circular log storing pending metadata operations (for crash recovery).
3. **FAT (File Allocation Table)**: An array of 32-bit integers tracking block allocation and file chains.
4. **Root Directory / Data Blocks**: The remaining space used for directory entries and actual file content.

### Technologies
- C++17
- POSIX API (`<pthread.h>` for `pthread_rwlock_t`, `<unistd.h>`, sockets for IPC)
- Binary I/O (`<fstream>`, `ftruncate`)

---

## Implementation Phases (Commit Strategy)

This project is split into 6 distinct phases. You can treat each phase as a separate day/week of commits.

### Phase 1: Core Disk Abstraction & Sparse File Initialization
*   **Goal**: Create the foundation for binary I/O and efficient disk formatting.
*   **Tasks**:
    *   Create `Disk.h` and `Disk.cpp` to abstract block-level I/O (`read_block`, `write_block`).
    *   Implement the `format` command using POSIX `ftruncate` to initialize a 1GB `virtual_disk.bin` sparse file instantly.
    *   Write the Superblock structure to block 0.
*   **Commit Theme**: "Initialize 1GB sparse virtual disk and superblock layout."

### Phase 2: FAT Implementation & Block Allocation
*   **Goal**: Implement the File Allocation Table to manage space.
*   **Tasks**:
    *   Create `FAT.h` and `FAT.cpp`.
    *   Implement logic to format the FAT array (marking blocks as free, EOF, or bad).
    *   Write functions to allocate new blocks (`allocate_block`) and free chains of blocks (`free_chain`).
*   **Commit Theme**: "Implement FAT allocation and block chaining."

### Phase 3: Directory Structure & File Metadata (Inodes)
*   **Goal**: Create the hierarchical directory structure.
*   **Tasks**:
    *   Define the `DirectoryEntry` struct (filename, attributes, starting block, size).
    *   Implement the root directory initialization.
    *   Implement traversal functions to find files by path.
*   **Commit Theme**: "Add directory entries and path traversal."

### Phase 4: Server Daemon & Interactive CLI Client
*   **Goal**: Build the client-server IPC architecture and basic file manipulation.
*   **Tasks**:
    *   Create `server.cpp` to run the file system as a daemon listening on a UNIX domain socket.
    *   Create `cli_client.cpp` with a read-eval-print loop (REPL) that sends commands to the server.
    *   Implement basic commands: `mkdir`, `rmdir`, `ls`, `cd`, `touch`, `rm`, `open`, `close`, `read`, `write`.
*   **Commit Theme**: "Develop IPC client-server architecture and POSIX-like system calls."

### Phase 5: Thread-Safe Readers-Writer Locks
*   **Goal**: Allow concurrent file access (100+ operations).
*   **Tasks**:
    *   Wrap file access and directory modification logic in the server with `pthread_rwlock_t`.
    *   Allow multiple client readers to execute `read` concurrently.
    *   Ensure `write`, `mkdir`, and `rm` acquire exclusive write locks.
    *   Create an automated stress-test script that spawns 100+ concurrent clients.
*   **Commit Theme**: "Integrate pthread readers-writer locks for concurrent server I/O."

### Phase 6: Circular Metadata Journaling
*   **Goal**: Prevent metadata corruption on crashes.
*   **Tasks**:
    *   Create `Journal.h` and `Journal.cpp`.
    *   Before any FAT or Directory modification, write an intent log entry to the Journal Area.
    *   On startup, read the Journal Area. If uncommitted logs exist, replay them (crash recovery).
*   **Commit Theme**: "Implement circular metadata journaling and crash recovery."

## Verification Plan

### Automated Tests
-   Write a bash script `stress_test.sh` that launches 100 background client processes performing random read/write/mkdir operations simultaneously to verify `pthread_rwlock_t` stability.
-   Simulate a crash by abruptly sending `SIGKILL` to the server process mid-write, then restart the server to verify the Journal replays the metadata correctly.

### Manual Verification
-   Run the CLI client, create deeply nested directories, write large files spanning multiple blocks, read them back, and verify the data matches.
