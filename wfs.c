#include <sys/types.h>
#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
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




// global variable
char *disk_image_path;
int global_fd;  // Global variable to store the file descriptor


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
    .rmdir = wfs_rmdir};

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
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
    close(global_fd);  // Close the fd when unmounting the filesystem
    argv++;
    argc--; 
    return fuse_main(argc, argv, &wfs_oper, NULL);
}
struct wfs_sb read_superblock(int fd) {
    struct wfs_sb sb;
    lseek(fd, 0, SEEK_SET);  // Move the read/write pointer to the start of the file.
    if (read(fd, &sb, sizeof(struct wfs_sb)) != sizeof(struct wfs_sb)) {
        perror("Failed to read superblock");
        exit(EXIT_FAILURE);  // Typically you'd handle errors more gracefully
    }
    return sb;
}
struct wfs_inode *read_inode(int fd, int inode_num, struct wfs_sb *sb) {
    struct wfs_inode *inode = malloc(sizeof(struct wfs_inode));
    if (!inode) {
        perror("Memory allocation failed for inode");
        exit(EXIT_FAILURE);
    }
    off_t inode_offset = sb->i_blocks_ptr + inode_num * sizeof(struct wfs_inode);
    lseek(fd, inode_offset, SEEK_SET);
    if (read(fd, inode, sizeof(struct wfs_inode)) != sizeof(struct wfs_inode)) {
        perror("Failed to read inode");
        free(inode);
        exit(EXIT_FAILURE);
    }
    return inode;
}
struct wfs_dentry *read_directory_block(int fd, off_t block_num) {
    size_t block_size = BLOCK_SIZE;  // Assuming a defined BLOCK_SIZE
    struct wfs_dentry *block = malloc(block_size);
    if (!block) {
        perror("Memory allocation failed for directory block");
        exit(EXIT_FAILURE);
    }
    off_t block_offset = block_num * block_size;
    lseek(fd, block_offset, SEEK_SET);
    if (read(fd, block, block_size) != block_size) {
        perror("Failed to read directory block");
        free(block);
        exit(EXIT_FAILURE);
    }
    return block;
}
struct wfs_inode *find_inode_by_path(const char *path, int fd) {
    struct wfs_sb sb = read_superblock(fd);
    struct wfs_inode *current_inode = read_inode(fd, 0, &sb);  // Start with root inode

    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");

    while (token != NULL && current_inode != NULL) {
        if (!S_ISDIR(current_inode->mode)) {
            fprintf(stderr, "Error: Not a directory\n");
            free(current_inode);
            free(path_copy);
            return NULL;
        }

        int found = 0;
        // Assume each directory uses a single block
        struct wfs_dentry *dentries = read_directory_block(fd, current_inode->blocks[0]);
        for (int i = 0; i < BLOCK_SIZE / sizeof(struct wfs_dentry); i++) {
            if (strcmp(dentries[i].name, token) == 0) {
                current_inode = read_inode(fd, dentries[i].num, &sb);
                found = 1;
                break;
            }
        }
        free(dentries);

        if (!found) {
            fprintf(stderr, "Error: Path not found\n");
            free(current_inode);
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
    struct wfs_inode *inode = find_inode_by_path(path,global_fd);
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
    (void)offset;
    (void)fi;
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    // Add actual file/directory names here
    return 0;
}

static int wfs_open(const char *path, struct fuse_file_info *fi)
{
    // Check if the file can be opened based on your FS logic
    return 0;
}

static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // Implement reading data from your FS structure
    return 0;
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // Implement writing data to your FS structure
    return 0;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    // Handle creation of non-directory files based on your FS logic
    return 0;
}

static int wfs_mkdir(const char *path, mode_t mode)
{
    // Handle directory creation based on your FS logic
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
