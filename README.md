# Custom FAT-Based Virtual File System

This project is a disk-backed FAT (File Allocation Table) virtual file system implemented in C++. It utilizes POSIX APIs for threading, synchronization, and binary I/O to simulate a persistent storage environment. The architecture follows a multi-phase approach, beginning with fundamental disk abstraction and culminating in a concurrent client-server model with metadata journaling.

## 🚀 Quick Start / How to Run

### 1. Build the Project
```bash
make clean && make
```
This compiles the core objects and generates three executables: `vfs_test`, `vfs_server`, and `vfs_client`.

### 2. Format the Virtual Disk
```bash
./vfs_test
```
*Must be run once before starting the server.* This initializes `virtual_disk.bin`, formats the FAT table, sets up the circular journal, and initializes the root directory.

### 3. Start the Server Daemon (Terminal 1)
```bash
./vfs_server
```
The server mounts the virtual disk, replays any pending journal transactions for crash recovery, and listens on `/tmp/vfs_socket`.

### 4. Connect the Interactive CLI Client (Terminal 2)
```bash
./vfs_client
```
Starts an interactive terminal shell with colored prompts. Example commands:
```text
vfs@fs:/$ mkdir docs
vfs@fs:/$ cd docs
vfs@fs:/docs$ write notes.txt Hello Virtual File System!
vfs@fs:/docs$ read notes.txt
Hello Virtual File System!
vfs@fs:/docs$ ls
notes.txt  (26 bytes)
vfs@fs:/docs$ cd /
vfs@fs:/$ exit
```

### 5. Run Concurrency Stress Test (Optional)
```bash
./stress_test.sh
```
Spawns 100 concurrent reader/writer client processes to verify thread safety and lock synchronization.

---

### Available CLI Commands

| Command | Syntax | Description |
|---|---|---|
| `ls` | `ls [path]` | List contents of current or specified directory |
| `cd` | `cd <path>` | Change working directory (validates existence on server) |
| `mkdir` | `mkdir <path>` | Create a new directory |
| `rmdir` | `rmdir <path>` | Remove an empty directory |
| `touch` | `touch <path>` | Create an empty file |
| `rm` | `rm <path>` | Remove a file |
| `write` | `write <path> <text>` | Write text content to a file (auto-creates file if missing) |
| `read` | `read <path>` | Display contents of a file |
| `help` | `help` | Show usage instructions |
| `exit` | `exit` | Disconnect from server |

---

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

## Phase 2: FAT Implementation & Block Allocation

In this phase, the File Allocation Table (FAT) is implemented to track and manage block allocation across the virtual disk, similar to how MS-DOS or early Windows systems managed files.

### 1. In-Memory FAT Caching
The FAT is essentially an array of 32-bit integers, where each index represents a block on the disk. Instead of reading the FAT from disk on every allocation, the `FAT` class caches the entire table in memory (requiring roughly 1MB of RAM for a 1GB disk). This allows for lightning-fast block lookups and chain traversals. Changes are written back to the physical disk blocks on a write-through basis using `flush_block()`.

### 2. System Block Reservation
During formatting, the FAT intelligently marks critical system structures as reserved (indicated by an `EOF` marker, `0xFFFFFFFF`). This ensures that the Superblock, the FAT array itself, the Journal, and the Root Directory blocks are never accidentally allocated to user files.

### 3. Allocation and Chaining
The FAT supports dynamically allocating free blocks and chaining them together to form larger files. `allocate_block()` performs a linear scan to find a free block (`0x00000000`), marks it as `EOF`, and returns its index. When a file is deleted, `free_chain()` traverses the FAT chain starting from the file's first block, efficiently marking all associated blocks as free and minimizing redundant disk writes by tracking "dirty" FAT blocks.

## Phase 3: Directory Structure & File Metadata

The Virtual File System (VFS) layer builds upon the Disk and FAT abstractions to create a hierarchical, user-facing directory structure.

### 1. The Directory Entry Struct
Files and directories are represented by a precisely packed 256-byte `DirectoryEntry` structure:
- **Filename**: Up to 243 characters.
- **Start Block**: The index of the first block in the FAT chain.
- **Size**: The total size of the file in bytes.
- **Attributes**: A byte flag indicating if the entry is a file, a directory, or unused.

By enforcing a strict 256-byte alignment, exactly 16 directory entries fit perfectly into a single 4096-byte block without fragmentation.

### 2. Path Resolution
The VFS can parse absolute paths (e.g., `/docs/hello.txt`) and resolve them to specific disk blocks. The `resolve_path()` function splits the path into components, reads the root directory, searches for the `docs` folder, and sequentially traverses the directory tree until the target file's metadata is located.

### 3. Dynamic Directory Expansion
Unlike simple file systems that limit the number of files in a directory, this VFS supports infinite directory growth. When `create_entry()` detects that a directory block is full, it automatically asks the FAT for a new block, links it to the existing directory chain, and initializes it with empty entries, providing seamless directory expansion.

### How to Compile and Test Phases 1-3
A unified test driver (`main.cpp`) is provided to verify the formatting, FAT chaining, directory creation, and path resolution simultaneously.

```bash
# Compile the project
make clean && make

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
Disk 'virtual_disk.bin' mounted successfully.
Formatting FAT... writing to disk.
VFS root directory formatted.

--- Testing VFS ---
Created directory 'docs'.
Created file 'hello.txt' inside 'docs'.

--- Testing Path Resolution ---
Resolved '/docs/hello.txt'. Start Block: 515

--- Testing Directory Listing ---
Root contains 1 entry.
'docs' contains 1 entry. Name: hello.txt

All VFS tests passed!
Disk 'virtual_disk.bin' unmounted.
```

## Phase 4: Server Daemon & Interactive CLI Client

This phase splits the file system into a robust Client-Server architecture, moving it closer to a true operating system driver model where a central daemon manages the hardware (virtual disk) while user applications communicate with it via system calls (IPC).

### 1. IPC Protocol
A tightly packed binary communication protocol (`Protocol.h`) defines how clients send requests to the server over a UNIX domain socket (`/tmp/vfs_socket`). The protocol encapsulates commands (`CMD_MKDIR`, `CMD_READ`, etc.), path strings, and raw binary data payloads in a structured header.

### 2. VFS Server Daemon
The `vfs_server` executable mounts the disk, initializes the FAT and VFS components, and binds to the UNIX domain socket. It operates as an infinite event loop, accepting connections from clients and dispatching their requests to the underlying VFS. The server handles all disk I/O, maintaining the integrity of the file system structure.

### 3. Interactive CLI Client
The `vfs_client` executable provides a Read-Eval-Print Loop (REPL) shell. It maintains a Current Working Directory (CWD) for the user, resolves relative paths, and provides familiar POSIX-like commands:
- `mkdir <path>`, `rmdir <path>`
- `touch <path>`, `rm <path>`
- `ls`
- `cd <path>`
- `read <path>`, `write <path> <text>`

### How to Compile and Test Phase 4
You must first run `./vfs_test` to ensure the disk is freshly formatted. Then, you can run the server in the background (or a separate terminal) and use the interactive client.

```bash
make clean && make

# 1. Format the disk
./vfs_test

# 2. Start the server in the background
./vfs_server &

# 3. Start the client shell
./vfs_client
```

Inside the client shell, you can execute commands like:
```text
vfs:/$ mkdir docs
Success.
vfs:/$ cd docs
vfs:/docs$ touch hello.txt
Success.
vfs:/docs$ write hello.txt Hello World!
Success.
vfs:/docs$ ls
- hello.txt 12
vfs:/docs$ read hello.txt
Hello World!
vfs:/docs$ exit
```

## Phase 5: Thread-Safe Readers-Writer Locks

To support concurrent file operations without corrupting the file allocation table or directory entries, the VFS server daemon employs multithreading and reader-writer locks.

### 1. Concurrent Connections
The server daemon spawns a new detached thread (`std::thread`) for every incoming client connection. This allows multiple clients to connect and send commands simultaneously without blocking each other.

### 2. Synchronization (`pthread_rwlock_t`)
A global Readers-Writer Lock protects the underlying Virtual File System:
- **Read Operations (`ls`, `read`)**: Acquire a read lock (`pthread_rwlock_rdlock`). Multiple clients can read from the disk concurrently, maximizing throughput for read-heavy workloads.
- **Write Operations (`mkdir`, `touch`, `write`, `rm`)**: Acquire an exclusive write lock (`pthread_rwlock_wrlock`). Only one client can modify the FAT or directory structure at a time, ensuring data integrity and preventing race conditions.

### How to Compile and Test Phase 5
A dedicated stress test script is provided to simulate heavy concurrent load. It formats the disk, starts the server in the background, and launches 100 concurrent clients (50 readers and 50 writers) simultaneously.

```bash
make clean && make

# Run the automated stress test
chmod +x stress_test.sh
./stress_test.sh
```

Expected output:
```text
Starting stress test...
Verifying results...
Successfully created and listed 50 files out of 50.
SUCCESS: Thread-safety verified! No race conditions detected.
Stress test complete.
```

## Phase 6: Circular Metadata Journaling

To prevent metadata corruption on sudden crashes or power failures, the Virtual File System uses physical intent logging, inspired by modern journaling file systems like ext3.

### 1. The Journal Area
During disk formatting, a segment of the virtual disk is reserved exclusively for the Journal (e.g., 256 blocks starting at Block 257). This area is treated as a circular intent log.

### 2. Transaction Lifecycle
Before modifying any FAT block or Directory Entry block, the file system writes the intent to the journal area in a two-step process:
- **Logging**: The targeted metadata block's new data is written to a data block in the Journal Area. A corresponding Journal Header block is also written, marking the state as `PENDING` along with the target block's actual address.
- **Committing**: Once the journal entry is secure on the disk, the VFS or FAT modifies the actual physical block on the disk. After the write succeeds, the Journal Header is updated to `COMMITTED`.

### 3. Crash Recovery (Replay)
When the VFS server daemon starts and mounts the virtual disk, it iterates through the Journal Area before serving any clients. 
If it discovers any `PENDING` transactions, it indicates that a crash occurred *after* logging but *before* or *during* the actual disk write. The server automatically recovers from this by reading the data from the journal and re-writing it to the target block on the disk, followed by marking the entry as `COMMITTED`. This ensures that the file system structure is always logically consistent.

### The Complete System
It is a fully functional, multithreaded, crash-resilient file system simulator that mimics the core concepts used in OS development.
