# Administrivia

- **Due date**: April 30th, 2024 at 11:59pm.
- **Handing it in**: 
  - Copy all your files to `~cs537-1/handin/<cslogin>/P7/`. Your `P7` folder should look like 
    ```
    P7/
    ├── mkfs.c
    ├── wfs.c
    ├── wfs.h
    └── ... (other header files, README, ... No need to submit Makefile)
    ```
    If you are working with a partner, only one student must copy the files to their handin directory.
- Late submissions
  - Projects may be turned in up to 3 days late but you will receive a penalty of 10 percentage points for every day it is turned in late.
  - Slip days: 
    - In case you need extra time on projects,  you each will have 2 slip days for individual projects and 3 slip days for group projects (5 total slip days for the semester). After the due date we will make a copy of the handin directory for on time grading.
    - To use a slip day you will submit your files with an additional file `slipdays.txt` in your regular project handin directory. This file should include one thing only and that is a single number, which is the number of slip days you want to use (ie. 1 or 2). Each consecutive day we will make a copy of any directories which contain one of these slipdays.txt files.
    - After using up your slip days you can get up to 90% if turned in 1 day late, 80% for 2 days late, and 70% for 3 days late. After 3 days we won't accept submissions.
    - Any exception will need to be requested from the instructors.
    - Example slipdays.txt
      ```
      1
      ```    
- Some tests will be provided at ~cs537-1/tests/P7. The tests will be partially complete and you are encouraged to create more tests.
- Questions: We will be using Piazza for all questions.
- Collaboration: The assignment can be done alone or in pairs. Copying code (from others) is considered cheating. [Read this](https://pages.cs.wisc.edu/~remzi/Classes/537/Spring2018/dontcheat.html) for more info on what is OK and what is not. Please help us all have a good semester by not doing this.
- This project is to be done on the [Linux lab machines](https://csl.cs.wisc.edu/docs/csl/2012-08-16-instructional-facilities/), so you can learn more about programming in C on a typical UNIX-based platform (Linux).  Your solution will be tested on these machines.

# Introduction

In this project, you'll create a filesystem using FUSE (Filesystem in Userspace). FUSE lets regular users build their own file systems without needing special permissions, opening up new possibilities for designing and using file systems. Your filesystem will handle basic tasks like reading, writing, making directories, deleting files, and more.

# Objectives

- To understand how filesystem operations are implemented.
- To implement a traditional block-based filesystem. 
- To learn to build a user-level filesystem using FUSE. 

# Background

## FUSE

FUSE (Filesystem in Userspace) is a powerful framework that enables the creation of custom filesystems in user space rather than requiring modifications to the kernel. This approach simplifies filesystem development and allows developers to create filesystems with standard programming languages like C, C++, Python, and others.

To use FUSE in a C-based filesystem, you define callback functions for various filesystem operations such as getattr, read, write, mkdir, and more. These functions are registered as handlers for specific filesystem actions and are invoked by the FUSE library when these actions occur. Callbacks are registered by installing them in the struct `fuse_operations.`

Here's an example demonstrating how to register a getattr function in a basic FUSE-based filesystem:

```c
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int my_getattr(const char *path, struct stat *stbuf) {
    // Implementation of getattr function to retrieve file attributes
    // Fill stbuf structure with the attributes of the file/directory indicated by path
    // ...

    return 0; // Return 0 on success
}

static struct fuse_operations ops = {
    .getattr = my_getattr,
    // Add other functions (read, write, mkdir, etc.) here as needed
};

int main(int argc, char *argv[]) {
    // Initialize FUSE with specified operations
    // Filter argc and argv here and then pass it to fuse_main
    return fuse_main(argc, argv, &ops, NULL);
}
```

This code demonstrates a basic usage of FUSE in C. The `my_getattr` function is an example of a callback function used to retrieve file attributes like permissions, size, and type. Other functions (like read, write, mkdir, etc.) can be similarly defined and added to `my_operations`. 

The `fuse_main` function initializes FUSE, passing the specified operations (in this case, my_operations) to handle various filesystem operations. This code structure allows you to define and register functions tailored to your filesystem's needs, enabling you to create custom filesystem behaviors using FUSE in C. 

`fuse_main` also accepts arguments which are passed to FUSE using `argc` and `argv`. Handle arguments specific to your program separately from those intended for FUSE. That is, you should filter out or process your program's arguments before passing `argc` and `argv` to `fuse_main`. If the first argument to your program is not meant to be passed to FUSE, then `argc` in `main()` would need to be decremented by one, and the elements of `argv` in `main()` would need to be shifted down by one as well.  

## Mounting

The mountpoint is a directory in the file system where the FUSE-based file system will be attached or "mounted." Once mounted, this directory serves as the entry point to access and interact with the FUSE file system. Any file or data within this mount point is associated with the FUSE file system, allowing users to read, write, and perform file operations as if they were interacting with a traditional disk.

A file can represent a container or virtual representation of a disk image. This file, when mounted to the mountpoint for the FUSE file system, effectively acts as a disk image, presenting a virtual disk within the file system.

When the FUSE file system is mounted on a file (such as an image file), it's as if the contents of that file become accessible as a disk or filesystem.

## Filesystem Details

We will create a simple filesystem similar to those you have seen in class such as FFS or ext2. Our filesystem will have a superblock, inode and data block bitmaps, and inodes and data blocks. There are two types of files in our filesystem -- directories and regular files. Data blocks for regular files hold the file data, while data blocks for directories hold directory entries. Each inode contains pointers to a fixed number of direct data blocks and a single indirect block to support larger files. You may presume the block size is always 512 bytes. The layout of a disk is shown below. 

![filesystem layout on disk](disk-layout.svg)

### Creating a file
To create a file, allocate a new inode using the inode bitmap. Then add a new directory entry to the parent inode, the inode of the directory which holds the new file. New files, whether they are regular files or directories, are initially empty. 

### Writes
To write to a file, find the data block corresponding to the offset being written to, and copy data from the write buffer into the data block(s). Note that writes may be split across data blocks, or span multiple data blocks. New data blocks should be allocated using the data block bitmap. 

### Reads
To read from a file, find the data block corresponding to the offset being read from, and copy data from the data block(s) to the read buffer. As with writes, reads may be split across data blocks, or span multiple data blocks.

### Removing files and directories
`unlink` and `rmdir` are responsible for deleting files and directories. To delete files, you should free (unallocate) any data blocks associated with the file, free it's inode, and remove the directory entry pointing to the file from the parent inode. 

Your implementation should use the structures provided in `wfs.h`. 

# Project details

You'll need to create the following C files for this project: 

- `mkfs.c`\
  This C program initializes a file to an empty filesystem. The program receives three arguments: the disk image file, the number of inodes in the filesystem, and the number of data blocks in the system. The number of blocks should always be rounded up to the nearest multiple of 32 to prevent the data structures on disk from being misaligned. For example:  
  ```sh
  ./mkfs -d disk_img -i 32 -b 200
  ```
  initializes the existing file `disk_img` to an empty filesystem with 32 inodes and 224 data blocks. The size of the inode and data bitmaps are determined by the number of blocks specified by `mkfs`. If `mkfs` finds that the disk image file is too small to accomodate the number of blocks, it should exit with return code 1. `mkfs` should write the superblock and root inode to the disk image. 
- `wfs.c`\
  This file contains the implementation for the FUSE filesystem. The bulk of your code will go in here. Running this program will mount the filesystem to a mount point, which are specifed by the arguments. The usage is 
  ```sh
  ./wfs disk_path [FUSE options] mount_point
  ```
  You need to pass `[FUSE options]` along with the `mount_point` to `fuse_main` as `argv`. You may assume `-s` is always passed to `mount.wfs` as a FUSE option to disable multi-threading. We recommend testing your program using the `-f` option, which runs FUSE in the foreground. With FUSE running in the foreground, you will need to open a second terminal to test your filesystem. In the terminal running FUSE, `printf` messages will be printed to the screen. You might want to have a `printf` at the beginning of every FUSE callback so you can see which callbacks are being run. 

## Features

Your filesystem needs to implement the following features: 

- Create empty files and directories
- Read and write to files, up to the maximum size supported by the indirect data block. 
- Read a directory (e.g. `ls` should work)
- Remove an existing file or directory (presume directories are empty)
- Get attributes of an existing file/directory\
  Fill the following fields of struct stat
  - st_uid
  - st_gid
  - st_atime
  - st_mtime
  - st_mode
  - st_size

Therefore, you need to fill the following fields of `struct fuse_operations`: 

```c
static struct fuse_operations ops = {
  .getattr = wfs_getattr,
  .mknod   = wfs_mknod,
  .mkdir   = wfs_mkdir,
  .unlink  = wfs_unlink,
  .rmdir   = wfs_rmdir,
  .read    = wfs_read,
  .write   = wfs_write,
  .readdir = wfs_readdir,
};
```

See https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201001/homework/fuse/fuse_doc.html to learn more about each registered function. 

## Structures

In `wfs.h`, we provide the structures used in this filesystem. It has been commented with details about each of them. In order for your filesystem to pass our tests, you should not modify the structures in this file.

## Utilities

To help you run your filesystem, we provided several scripts: 

- `create_disk.sh` creates a file named `disk` with size 1MB whose content is zeroed. You can use this file as your disk image. We may test your filesystem with images of different sizes, so please do not assume the image is always 1MB.
- `umount.sh` unmounts a mount point whose path is specified in the first argument. 
- `Makefile` is a template makefile used to compile your code. It will also be used for grading. Please make sure your code can be compiled using the commands in this makefile. 

A typical way to compile and launch your filesystem is: 

```sh
$ make
$ ./create_disk.sh                 # creates file `disk.img`
$ ./mkfs -d disk.img -i 32 -b 200  # initialize `disk.img`
$ mkdir mnt
$ ./wfs disk.img -f -s mnt             # mount. -f runs FUSE in foreground
```

You should be able to interact with your filesystem once you mount it: 

```sh
$ stat mnt
$ mkdir mnt/a
$ stat mnt/a
$ mkdir mnt/a/b
$ ls mnt
$ echo asdf > mnt/x
$ cat mnt/x
```

## Error handling

If any of the following issues occur during the execution of a registered function, it's essential to return the respective error code. These error code macros are accessible by including header `<errno.h>`.

- File/directory does not exist while trying to read/write a file/directory\
  return `-ENOENT`
- A file or directory with the same name already exists while trying to create a file/directory\
  return `-EEXIST`
- There is insufficient disk space while trying to create or write to a file\
  return `-ENOSPC`

## Debugging

### Inspect superblock
You can see the exact contents present in the disk image, before mounting. For a new disk image, the disk should contain the superblock after running `mkfs`. 

```sh
$ ./create_disk.sh
$ xxd -e -g 4 disk | less
$ ./mkfs disk
$ xxd -e -g 4 disk | less
```

### Printing
Your filesystem will print to `stdout` if it is running in the foreground (`-f`). We recommend adding a print statement to the beginning of each callback function so that you know which functions are being called (e.g. watch how many times `getattr` is called). You may, of course, add more detailed print statements to debug further.

### Debugger
To run the filesystem in `gdb`, use `gdb --args ./wfs disk.img -f -s mnt`, and type `run` once inside `gdb`.

## Important Notes

1. Manually inspecting your filesystem (see Debugging section above), before running the tests, is highly encouraged. You should also experiment with your filesystem using simple utilities such as `mkdir`, `ls`, `touch`, `echo`, `rm`, etc. 
2. Directories will not use the indirect block. That is, directories entries will be limited to the number that can fit in the direct data blocks.
3. Directories may contain blank entries up to the size as marked in the directory inode. That is, a directory with size 512 bytes will use one data block, both of the following entry layouts would be legal -- 15 blank entries followed by a valid directory entry, or a valid directory entry followed by 15 blank entries. You should free all directory data blocks with `rmdir`, but you do not need to free directory data blocks when unlinking files in a directory.
4. A valid file/directory name consists of letters (both uppercase and lowercase), numbers, and underscores (_). Path names are always separated by forward-slash. You do not need to worry about escape sequences for other characters.
5. The maximum file name length is 28
6. Please make sure your code can be compiled using the commands in the provided Makefile.
7. We recommend using `mmap` to map the entire disk image into memory when the filesystem is mounted. Mapping the image into memory simplifies reading and writing the on-disk structures a great deal, compared to `read` and `write` system calls.
8. Think carefully about the interfaces you will need to build to manipulate on-disk data structures. For example, you might have an `allocate_inode()` function which allocates an inode using the bitmap and returns a pointer to a new inode, or returns an error if there are no more inodes available in the system.
9. You must use the superblock and inode structs as they are defined in the header file, but the actual allocation and free strategies are up to you. Our tests will evaluate whether or not you have the correct number of blocks allocated, but we do not assume they are in a particular location on-disk.

# Further Reading and References

* https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201001/homework/fuse/fuse_doc.html
* https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/html/index.html
* http://libfuse.github.io/doxygen/index.html
* https://github.com/fuse4x/fuse/tree/master/example
* `/usr/include/asm-generic/errno-base.h`
