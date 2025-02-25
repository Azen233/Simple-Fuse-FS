# Simple-FUSE-FS

*A lightweight, user-space filesystem built with FUSE in C.*

---

## Overview

**Simple-FUSE-FS** is a minimalistic, block-based filesystem designed to run entirely in user space using [FUSE (Filesystem in Userspace)](https://libfuse.github.io/). It supports standard file operations such as creating, reading, writing, and deleting files and directories. This project serves as an excellent starting point for anyone looking to explore filesystem design without the complexity of kernel development.

---

## Features

- **FUSE-based Filesystem:** Runs in user space, eliminating the need for kernel modifications.
- **Block-based Storage:** Utilizes a traditional block-based layout with inodes, bitmaps, and data blocks.
- **Disk Image Handling:** Uses `mmap` for efficient access and manipulation of a virtual disk image.
- **Basic File Operations:** Supports file/directory creation, deletion, reading, and writing.
- **Error Handling:** Implements robust error codes using standard `errno` macros.
- **Modular Design:** Easy to extend and adapt for additional features or customizations.

---

## Installation & Setup

### Prerequisites

- **FUSE Library:** Make sure FUSE is installed on your system.
  - On Ubuntu/Debian:  
    ```sh
    sudo apt-get install libfuse-dev fuse
    ```
  - On macOS, you may need to install [macFUSE](https://osxfuse.github.io/).

### Clone the Repository

```sh
git clone https://github.com/YOUR_USERNAME/simple-fuse-fs.git
cd simple-fuse-fs
# Build the Project
Compile the filesystem with:

```sh
make
```

This will compile the source code located in the `src/` directory and generate the necessary binaries (e.g., `mkfs` and `wfs`).

## Create and Format a Disk Image
A helper script (`create_disk.sh`) is provided to create a zeroed disk image. To create and format your disk image:

```sh
./create_disk.sh       # Creates a 1MB disk image (e.g., disk.img)
./mkfs -d disk.img -i 32 -b 200   # Initializes disk.img with 32 inodes and 200 data blocks
```

Note: The number of data blocks is automatically rounded up to a multiple of 32 for proper alignment.

## Mount the Filesystem
Create a mount point and mount the filesystem using:

```sh
mkdir mnt
./wfs disk.img -f -s mnt  # The -f flag runs FUSE in the foreground; -s disables multithreading
```

Once mounted, you can interact with the filesystem as if it were a physical disk.

## Testing Basic Commands
After mounting, try the following commands:

```sh
mkdir mnt/example_dir
echo "Hello, Simple-FUSE-FS!" > mnt/example_file
ls -l mnt
cat mnt/example_file
rm mnt/example_file
```

## Unmount the Filesystem
When finished, unmount with:

```sh
fusermount -u mnt
```

On macOS, you may need to use:

```sh
umount mnt
```

## How It Works
Simple-FUSE-FS emulates a traditional UNIX filesystem by managing a virtual disk image. Key components include:

- **Inodes:** Data structures that store metadata for files and directories (permissions, ownership, timestamps, size, etc.).
- **Bitmaps:** Used to track free and allocated inodes and data blocks.
- **Data Blocks:** Fixed-size blocks (default 512 bytes) where file contents or directory entries are stored.
- **Direct & Indirect Block Pointers:** Support for small and large files by linking data blocks.
- **Disk Image:** A file that serves as a virtual disk, mapped into memory using `mmap` for efficient I/O operations.
- **FUSE Callbacks:** Functions registered with FUSE (e.g., `getattr`, `readdir`, `read`, `write`) to handle filesystem operations.

