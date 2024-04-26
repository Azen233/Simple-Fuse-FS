#include <sys/types.h>
#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h> 
#include <unistd.h>

static int wfs_getattr(const char *path, struct stat *stbuf);
static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int wfs_open(const char *path, struct fuse_file_info *fi);
static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int wfs_mknod(const char *path, mode_t mode, dev_t dev);
static int wfs_mkdir(const char *path, mode_t mode);
static int wfs_unlink(const char *path);
static int wfs_rmdir(const char *path);




// Map functions to fuse_operations
static struct fuse_operations wfs_oper = {
    .getattr = wfs_getattr,
    .readdir = wfs_readdir,
    .open = wfs_open,
    .read = wfs_read,
    .write = wfs_write,
    .mknod = wfs_mknod,
    .mkdir = wfs_mkdir,
    .unlink = wfs_unlink,
    .rmdir = wfs_rmdir
};

// Global variables
char *disk_image_path;
int global_fd;
void *mapped_memory;
struct wfs_sb sb;

// Function prototypes
struct wfs_inode *find_inode_by_path(const char *path);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <disk_path> [FUSE options] <mount_point>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Store the disk image path in a global variable
    disk_image_path = argv[1];
    global_fd = open(disk_image_path, O_RDWR);
    if (global_fd == -1) {
        perror("Failed to open disk image");
        exit(EXIT_FAILURE);
    }

    // Get file status to determine file size
    struct stat file_stat;
    if (fstat(global_fd, &file_stat) == -1) {
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    // Map the entire file into memory
    mapped_memory = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, global_fd, 0);
    if (mapped_memory == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Close the file descriptor
    close(global_fd);

    // Set superblock pointer
    sb = *((struct wfs_sb *)mapped_memory);

    // Pass the modified argv and argc to fuse_main
    argv++;
    argc--;

    // Call fuse_main with modified arguments
    int fuse_ret = fuse_main(argc, argv, &wfs_oper, NULL);

    // Unmap the memory
    if (munmap(mapped_memory, file_stat.st_size) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    return fuse_ret;
}

struct wfs_inode *find_inode_by_path(const char *path) {
    // Start with the root inode
    struct wfs_inode *current_inode = (struct wfs_inode *)((char *)mapped_memory + sb.i_blocks_ptr);

    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");

    while (token != NULL && current_inode != NULL) {
        if (!S_ISDIR(current_inode->mode)) {
            fprintf(stderr, "Error: Not a directory\n");
            free(path_copy);
            return NULL;
        }

        int found = 0;
        // Iterate through directory entries in the data blocks
        for (int i = 0; i < N_BLOCKS - 1 && current_inode->blocks[i] != 0; i++) {
            struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + current_inode->blocks[i] * BLOCK_SIZE);
            for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
                if (strcmp(dentries[j].name, token) == 0) {
                    // Found the directory entry
                    current_inode = (struct wfs_inode *)((char *)mapped_memory + sb.i_blocks_ptr + dentries[j].num * sizeof(struct wfs_inode));
                    found = 1;
                    break;
                }
            }
            if (found) break;
        }

        if (!found) {
            fprintf(stderr, "Error: Path not found\n");
            free(path_copy);
            return NULL;
        }

        token = strtok(NULL, "/");
    }

    free(path_copy);
    return current_inode;
}


static int wfs_getattr(const char *path, struct stat *stbuf) {
    // Clear out the stat buffer
    memset(stbuf, 0, sizeof(struct stat));

    // Find the inode for the given path
    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode) {
        // If the inode was not found, return an error
        return -ENOENT;
    }

    // Set the appropriate fields in stbuf from the inode
    stbuf->st_mode = inode->mode;
    stbuf->st_nlink = inode->nlinks;
    stbuf->st_size = inode->size;
    stbuf->st_uid = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_atime = inode->atim;
    stbuf->st_mtime = inode->mtim;
    stbuf->st_ctime = inode->ctim;

    // Set the number of 512-byte blocks used by this inode
    stbuf->st_blocks = inode->size / 512 + (inode->size % 512 ? 1 : 0);

    return 0;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    // Find the inode corresponding to the given path
    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode || !S_ISDIR(inode->mode)) {
        // If the inode is not found or is not a directory, return an error
        return -ENOENT;
    }

    // Read the directory entries from the inode's data blocks and fill the buffer
    for (int i = 0; i < N_BLOCKS - 1 && inode->blocks[i] != 0; i++) {
        struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + inode->blocks[i] * BLOCK_SIZE);
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
            // Check if the directory entry is valid
            if (dentries[j].num != 0) {
                // Add the directory entry to the buffer using the filler function
                filler(buf, dentries[j].name, NULL, 0);
            }
        }
    }

    return 0;
}




static int wfs_open(const char *path, struct fuse_file_info *fi) {
    // Check if the file can be opened based on your FS logic
    // For demonstration purposes, let's assume we are checking if the file exists
    global_fd = open(path, O_RDONLY);
    if (global_fd == -1) {
        // File does not exist or cannot be opened
        return -errno; // Return the appropriate error code
    }

    // Indicate that the file was successfully opened
    return 0;
}

static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Implement reading data from your FS structure
    // Here, we'll read from the file descriptor stored in the global variable global_fd

    // First, obtain the file descriptor from the global variable
    int fd = global_fd;
    if (fd == -1) {
        // File descriptor not valid, return an error
        return -EBADF;
    }

    // Seek to the specified offset in the file
    if (lseek(fd, offset, SEEK_SET) == -1) {
        // Error while seeking
        return -errno;
    }

    // Read data from the file into the buffer
    ssize_t bytes_read = read(fd, buf, size);
    if (bytes_read == -1) {
        // Error while reading
        return -errno;
    }

    // Return the number of bytes read
    return bytes_read;
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // Find the inode corresponding to the given path
    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode || S_ISDIR(inode->mode)) {
        // If the inode is not found or is a directory, return an error
        return -ENOENT;
    }

    // Check if the file is open for writing
    if (!(fi->flags & O_WRONLY) && !(fi->flags & O_RDWR)) {
        // File is not open for writing
        return -EACCES;
    }

    // Calculate the total size of the data to write
    size_t total_size = size + offset;

    // Check if the inode has enough space to accommodate the data
    if (total_size > N_BLOCKS * BLOCK_SIZE) {
        // Data exceeds the maximum file size
        return -ENOSPC;
    }

    // Write the data to the inode's data blocks
    off_t block_offset = offset / BLOCK_SIZE;
    off_t block_remainder = offset % BLOCK_SIZE;
    size_t bytes_written = 0;

    while (bytes_written < size) {
        // Calculate the block index and offset within the block
        off_t block_index = block_offset % N_BLOCKS;
        off_t block_offset_within_block = block_remainder;

        // Calculate the remaining space in the block
        size_t space_remaining_in_block = BLOCK_SIZE - block_offset_within_block;

        // Determine the number of bytes to write in this iteration
        size_t bytes_to_write = (size - bytes_written) > space_remaining_in_block ? space_remaining_in_block : (size - bytes_written);

        // Write the data to the block
        memcpy((char *)mapped_memory + inode->blocks[block_index] * BLOCK_SIZE + block_offset_within_block, buf + bytes_written, bytes_to_write);

        // Update counters
        bytes_written += bytes_to_write;
        block_offset++;
        block_remainder = 0; // After the first iteration, remainder will be 0
    }

    // Update the inode's size if necessary
    if (total_size > inode->size) {
        inode->size = total_size;
    }

    // Update the inode's modification time
    inode->mtim = time(NULL);

    return bytes_written; // Return the number of bytes actually written
}

static int wfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    // Find the parent directory inode
    struct wfs_inode *parent_inode = find_inode_by_path(path);
    if (!parent_inode || !S_ISDIR(parent_inode->mode)) {
        // If parent directory does not exist or is not a directory, return error
        return -ENOENT;
    }

    // Extract the name of the new file from the path
    const char *file_name = strrchr(path, '/');
    if (file_name == NULL) {
        // No '/' found in the path, so the path is incorrect
        return -EINVAL;
    }
    file_name++; // Move past the '/'

    // Check if the parent directory is full
    if (parent_inode->size >= N_BLOCKS * BLOCK_SIZE) {
        // Parent directory is full, return error
        return -ENOSPC;
    }

    // Create a new inode for the file
    struct wfs_inode new_inode;
    memset(&new_inode, 0, sizeof(struct wfs_inode));
    new_inode.mode = mode;
    new_inode.uid = getuid();
    new_inode.gid = getgid();
    new_inode.size = 0;
    new_inode.atim = time(NULL);
    new_inode.mtim = time(NULL);
    new_inode.ctim = time(NULL);

    // Allocate space for the new inode
    off_t new_inode_offset = sb.i_blocks_ptr + sb.num_inodes * sizeof(struct wfs_inode);
    memcpy((char *)mapped_memory + new_inode_offset, &new_inode, sizeof(struct wfs_inode));

    // Add a directory entry for the new file in the parent directory
    struct wfs_dentry new_dentry;
    strncpy(new_dentry.name, file_name, MAX_NAME);
    new_dentry.num = sb.num_inodes; // Index of the new inode
    off_t parent_dir_offset = parent_inode->blocks[0]; // Assume parent directory has only one block
    memcpy((char *)mapped_memory + parent_dir_offset + parent_inode->size, &new_dentry, sizeof(struct wfs_dentry));
    parent_inode->size += sizeof(struct wfs_dentry); // Update parent directory size

    // Update the superblock and parent directory's modification time
    sb.num_inodes++;
    parent_inode->mtim = time(NULL);

    return 0; // Return success
}


static int wfs_mkdir(const char *path, mode_t mode) {
    // Find the parent directory inode
    struct wfs_inode *parent_inode = find_inode_by_path(path);
    if (!parent_inode) {
        // If parent directory does not exist, return error
        return -ENOENT;
    }

    // Create a new directory inode
    struct wfs_inode new_dir_inode;
    memset(&new_dir_inode, 0, sizeof(struct wfs_inode));
    new_dir_inode.mode = mode | S_IFDIR; // Set mode to directory
    new_dir_inode.nlinks = 2; // . and .. links
    new_dir_inode.uid = getuid();
    new_dir_inode.gid = getgid();
    new_dir_inode.size = 0;
    new_dir_inode.atim = time(NULL);
    new_dir_inode.mtim = time(NULL);
    new_dir_inode.ctim = time(NULL);

    // Allocate space for the new inode
    off_t new_inode_offset = sb.i_blocks_ptr + sb.num_inodes * sizeof(struct wfs_inode);
    memcpy((char *)mapped_memory + new_inode_offset, &new_dir_inode, sizeof(struct wfs_inode));

    // Extract directory name from path
    const char *dir_name = strrchr(path, '/');
    if (dir_name == NULL) {
        dir_name = path; // Root directory
    } else {
        dir_name++; // Move past the '/'
    }

    // Check if parent directory is full
    if (parent_inode->size >= N_BLOCKS * BLOCK_SIZE) {
        // Parent directory is full, return error
        return -ENOSPC;
    }

    // Add directory entry to the parent directory
    struct wfs_dentry new_dir_entry;
    strncpy(new_dir_entry.name, dir_name, MAX_NAME);
    new_dir_entry.num = sb.num_inodes; // Index of the new directory's inode
    off_t parent_dir_offset = parent_inode->blocks[0]; // Assume parent directory has only one block
    memcpy((char *)mapped_memory + parent_dir_offset + parent_inode->size, &new_dir_entry, sizeof(struct wfs_dentry));
    parent_inode->size += sizeof(struct wfs_dentry); // Update parent directory size

    // Update superblock
    sb.num_inodes++;

    // Update parent directory's modification time
    parent_inode->mtim = time(NULL);

    return 0;
}


static int wfs_unlink(const char *path)
{
    // Handle file deletion based on your FS logic
    return 0;
}

static int wfs_rmdir(const char *path)
{
    // Handle directory removal based on your FS logic
    return 0;
}
