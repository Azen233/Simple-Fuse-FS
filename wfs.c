#include <sys/types.h>
#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

int wfs_init(size_t num_inodes, size_t num_data_blocks, void *memory_start);
static int wfs_getattr(const char *path, struct stat *stbuf);
static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
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
    .read = wfs_read,
    .write = wfs_write,
    .mknod = wfs_mknod,
    .mkdir = wfs_mkdir,
    .unlink = wfs_unlink,
    .rmdir = wfs_rmdir};

// Global variables
char *disk_image_path;
int global_fd;
void *mapped_memory;
struct wfs_sb sb;
struct wfs_inode *inodes;
char *inode_bitmap;
char *data_bitmap;
char *data_blocks; // Pointer to the data blocks section
// Function prototypes
struct wfs_inode *find_inode_by_path(const char *path);
int allocate_inode();
int allocate_block();
static int add_directory_entry(struct wfs_inode *parent_inode, int new_inode_num, const char *new_entry_name);
static int remove_directory_entry(struct wfs_inode *parent_inode, int inode_num, const char *entry_name);
static void free_block(int block_num);
static void free_inode(int inode_num);

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <disk_path> [FUSE options] <mount_point>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Store the disk image path in a global variable
    disk_image_path = argv[1];
    global_fd = open(disk_image_path, O_RDWR);
    if (global_fd == -1)
    {
        perror("Failed to open disk image");
        exit(EXIT_FAILURE);
    }

    // Get file status to determine file size
    struct stat file_stat;
    if (fstat(global_fd, &file_stat) == -1)
    {
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    // Map the entire file into memory
    mapped_memory = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, global_fd, 0);
    if (mapped_memory == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Close the file descriptor
    close(global_fd);

    // Set superblock pointer
    sb = *((struct wfs_sb *)mapped_memory);
    inode_bitmap = (char *)mapped_memory + sb.i_bitmap_ptr;
    data_bitmap = (char *)mapped_memory + sb.d_bitmap_ptr;
    inodes = (struct wfs_inode *)((char *)mapped_memory + sb.i_blocks_ptr);
    data_blocks = (char *)mapped_memory + sb.d_blocks_ptr; // Initialize pointer to data blocks

    inode_bitmap[0] |= 0x01;
    // Pass the modified argv and argc to fuse_main
    argv++;
    argc--;

    // Call fuse_main with modified arguments
    int fuse_ret = fuse_main(argc, argv, &wfs_oper, NULL);

    // Unmap the memory
    if (munmap(mapped_memory, file_stat.st_size) == -1)
    {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    return fuse_ret;
}


struct wfs_inode *find_inode_by_path(const char *path)
{
    printf("find node by path for %s\n", path);
    if (strcmp(path, "/") == 0) {
        return (struct wfs_inode *)((char *)mapped_memory + sb.i_blocks_ptr); // Return root inode directly
    }

    struct wfs_inode *current_inode = (struct wfs_inode *)((char *)mapped_memory + sb.i_blocks_ptr);
    char *path_copy = strdup(path);
    if (!path_copy) {
        perror("strdup failed");
        return NULL;
    }

    char *token = strtok(path_copy, "/");
    while (token != NULL) {
        if (!S_ISDIR(current_inode->mode)) {
            fprintf(stderr, "Not a directory\n");
            free(path_copy);
            return NULL;
        }

        bool found = false;
        // Check direct blocks
        for (int i = 0; i < D_BLOCK; i++) {
            if (current_inode->blocks[i] != 0) {
                struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + current_inode->blocks[i] );
                for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
                    if (strcmp(dentries[j].name, token) == 0) {
                        current_inode = (struct wfs_inode *)((char *)mapped_memory + sb.i_blocks_ptr + dentries[j].num * BLOCK_SIZE);
                        found = true;
                        break;
                    }
                }
            }
            if (found) break;
        }

        // If not found in direct blocks, check indirect block
        if (!found && current_inode->blocks[IND_BLOCK] != 0) {
            off_t *indirect_blocks = (off_t *)((char *)mapped_memory + current_inode->blocks[IND_BLOCK] );
            for (int k = 0; k < BLOCK_SIZE / sizeof(off_t) && indirect_blocks[k] != 0; k++) {
                struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + indirect_blocks[k] );
                for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
                    if (strcmp(dentries[j].name, token) == 0) {
                        current_inode = (struct wfs_inode *)((char *)mapped_memory + sb.i_blocks_ptr + dentries[j].num * BLOCK_SIZE);
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        if (!found) {
            fprintf(stderr, "Path component %s not found\n", token);
            free(path_copy);
            return NULL;
        }

        token = strtok(NULL, "/");
    }

    free(path_copy);
    return current_inode; // Return the inode found at the end of the path
}

static int wfs_getattr(const char *path, struct stat *stbuf)
{
    // Clear out the stat buffer
    printf("get attribute of%s\n", path);
    memset(stbuf, 0, sizeof(struct stat));
    // Find the inode for the given path
    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode)
    {
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

    printf("num:%dmode: %o, size: %ld\n", inode->num, inode->mode, inode->size);
    return 0;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("readdir...\n");
    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode || !S_ISDIR(inode->mode))
    {
        return -ENOENT; // Inode not found or not a directory
    }

    // Read from direct blocks first
    for (int i = 0; i < D_BLOCK && inode->blocks[i] != 0; i++)
    {
        struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + inode->blocks[i]);
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
        {
            if (dentries[j].num != 0) // Valid entry
            {
                if (filler(buf, dentries[j].name, NULL, 0) != 0) 
                    return 0; // Buffer full or other filler-related error
            }
        }
    }

    // Handle indirect block
    if (inode->blocks[IND_BLOCK] != 0)
    {
        off_t *indirect_blocks = (off_t *)((char *)mapped_memory + inode->blocks[IND_BLOCK]);
        for (int i = 0; i < BLOCK_SIZE / sizeof(off_t) && indirect_blocks[i] != 0; i++)
        {
            struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + indirect_blocks[i]);
            for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
            {
                if (dentries[j].num != 0) // Valid entry
                {
                    if (filler(buf, dentries[j].name, NULL, 0) != 0) 
                        return 0; // Buffer full or other filler-related error
                }
            }
        }
    }

    return 0; 
}



static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("Reading from path: %s\n", path);

    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode)
    {
        fprintf(stderr, "Error finding inode for path %s\n", path);
        return -ENOENT;
    }

    if (offset >= inode->size)
    {
        return 0;  // Nothing to read, offset is beyond the end of the file
    }

    size_t bytes_to_read = min(size, inode->size - offset);
    size_t bytes_read = 0;
    int block_index = offset / BLOCK_SIZE;
    int block_offset = offset % BLOCK_SIZE;

    while (bytes_read < bytes_to_read)
    {
        size_t current_block_index;
        if (block_index < D_BLOCK)  // Direct block
        {
            current_block_index = inode->blocks[block_index];
        }
        else if (inode->blocks[IND_BLOCK] != 0)  // Indirect block
        {
            int *indirect_blocks = (int *)((char *)mapped_memory + inode->blocks[IND_BLOCK]);
            current_block_index = indirect_blocks[block_index - D_BLOCK];
        }
        else
        {
            fprintf(stderr, "Error: Indirect block not initialized.\n");
            return -EIO;  // Should not happen, indicates a corruption or logic error
        }

        char *block_data = (char *)mapped_memory + current_block_index;
        size_t bytes_from_block = min(BLOCK_SIZE - block_offset, bytes_to_read - bytes_read);
        memcpy(buf + bytes_read, block_data + block_offset, bytes_from_block);
        
        bytes_read += bytes_from_block;
        block_index++;
        block_offset = 0;  // Only the first block might start at an offset
    }

    return bytes_read;
}



int allocate_block()
{
    // Access the data bitmap directly from the global variable
    char *bitmap = data_bitmap;

    // Iterate over the bitmap to find a free block, using the number of data blocks from the global superblock
    for (size_t i = 0; i < sb.num_data_blocks; i++)
    {
        size_t byte_index = i / 8;
        size_t bit_index = i % 8;

        // Check if the current block is free
        if (!(bitmap[byte_index] & (1 << bit_index)))
        {
            // Mark the block as used
            bitmap[byte_index] |= (1 << bit_index);

            // Optionally clear the block in data storage if necessary
            memset((char *)mapped_memory + sb.d_blocks_ptr + i * BLOCK_SIZE, 0, BLOCK_SIZE);

            // Return the offset from the beginning of the data blocks section
            return sb.d_blocks_ptr + i * BLOCK_SIZE;
        }
    }

    // Return -1 if no free blocks are available
    return -1;
}
int initialize_indirect_block(struct wfs_inode *inode) 
{
    int indirect_block_index = allocate_block();
    if (indirect_block_index == -1) return -ENOSPC; 

    inode->blocks[IND_BLOCK] = indirect_block_index;  // Set indirect block index
    off_t *block_pointers = (off_t *)((char *)mapped_memory + sb.d_blocks_ptr + indirect_block_index);
    memset(block_pointers, 0, BLOCK_SIZE);  // Initialize all entries to zero
    return 0;
}



int allocate_inode() 
{
    char *bitmap = inode_bitmap;
    for (size_t i = 1; i < sb.num_inodes; i++) 
    {
        size_t byte_index = i / 8;
        size_t bit_index = i % 8;

        if (!(bitmap[byte_index] & (1 << bit_index))) 
        {
            bitmap[byte_index] |= (1 << bit_index);

            struct wfs_inode *new_inode = &inodes[i];
            memset(new_inode, 0, sizeof(struct wfs_inode));  // Zero out the new inode
            new_inode->num = i;
            new_inode->nlinks = 1; // Default link count
            new_inode->atim = new_inode->mtim = new_inode->ctim = time(NULL); // Initialize times

            // Initialize all direct and indirect block pointers to 0 (no block assigned)
            for (int j = 0; j < N_BLOCKS; j++) 
            {
                new_inode->blocks[j] = 0;
            }

            return i;
        }
    }
    return -1; // No free inodes available
}


static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode) return -ENOENT;
    if (!S_ISREG(inode->mode)) return -EISDIR;

    off_t end_offset = offset + size;
    if (end_offset > inode->size) {
        inode->size = end_offset;
        inode->mtim = time(NULL);
    }

    size_t bytes_written = 0;

    for (size_t i = offset / BLOCK_SIZE; i <= (end_offset - 1) / BLOCK_SIZE; i++) {
        if (i < D_BLOCK) {
            // Handling direct blocks
            if (inode->blocks[i] == 0) {
                inode->blocks[i] = allocate_block();
                if (inode->blocks[i] == -1) return -ENOSPC;
            }
            char *block_data = (char *)mapped_memory + inode->blocks[i];
            size_t block_start = (i == offset / BLOCK_SIZE) ? (offset % BLOCK_SIZE) : 0;
            size_t block_end = (i == (end_offset - 1) / BLOCK_SIZE) ? ((end_offset - 1) % BLOCK_SIZE + 1) : BLOCK_SIZE;
            memcpy(block_data + block_start, buf + bytes_written, block_end - block_start);
            bytes_written += block_end - block_start;
        } else {
            // Handling indirect blocks
            if (inode->blocks[IND_BLOCK] == 0) {
                if (initialize_indirect_block(inode) != 0) return -ENOSPC;
            }
            off_t *indirect_blocks = (off_t *)((char *)mapped_memory + inode->blocks[IND_BLOCK]);
            if (indirect_blocks[i - D_BLOCK] == 0) {
                indirect_blocks[i - D_BLOCK] = allocate_block();
                if (indirect_blocks[i - D_BLOCK] == -1) return -ENOSPC;
            }
            char *block_data = (char *)mapped_memory + indirect_blocks[i - D_BLOCK];
            size_t block_start = (i == offset / BLOCK_SIZE) ? (offset % BLOCK_SIZE) : 0;
            size_t block_end = (i == (end_offset - 1) / BLOCK_SIZE) ? ((end_offset - 1) % BLOCK_SIZE + 1) : BLOCK_SIZE;
            memcpy(block_data + block_start, buf + bytes_written, block_end - block_start);
            bytes_written += block_end - block_start;
        }
    }

    return bytes_written;
}


static int add_directory_entry(struct wfs_inode *parent_inode, int new_inode_num, const char *new_entry_name)
{
    // Attempt to add in direct blocks first
    for (int i = 0; i < D_BLOCK; i++) {
        if (parent_inode->blocks[i] == 0) {
            parent_inode->blocks[i] = allocate_block();
            if (parent_inode->blocks[i] == -1) return -ENOSPC; // No space left
            memset((char *)mapped_memory + parent_inode->blocks[i], 0, BLOCK_SIZE);
        }
        struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + parent_inode->blocks[i]);
        for (int j = 0; j < (BLOCK_SIZE / sizeof(struct wfs_dentry)); j++) {
            if (dentries[j].num == 0) {
                strncpy(dentries[j].name, new_entry_name, MAX_NAME - 1);
                dentries[j].num = new_inode_num;
                return 0; // Success
            }
        }
    }

    // Initialize indirect block if necessary
    if (parent_inode->blocks[IND_BLOCK] == 0) {
        if (initialize_indirect_block(parent_inode) == -ENOSPC) return -ENOSPC;
    }

    // Add entry in indirect block
    off_t *indirect_blocks = (off_t *)((char *)mapped_memory + parent_inode->blocks[IND_BLOCK]);
    for (int i = 0; i < BLOCK_SIZE / sizeof(off_t); i++) {
        if (indirect_blocks[i] == 0) {
            indirect_blocks[i] = allocate_block();
            if (indirect_blocks[i] == -1) return -ENOSPC;
            memset((char *)mapped_memory + indirect_blocks[i], 0, BLOCK_SIZE);
        }
        struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + indirect_blocks[i]);
        for (int j = 0; j < (BLOCK_SIZE / sizeof(struct wfs_dentry)); j++) {
            if (dentries[j].num == 0) {
                strncpy(dentries[j].name, new_entry_name, MAX_NAME - 1);
                dentries[j].num = new_inode_num;
                return 0; // Success
            }
        }
    }

    // If all blocks are full
    return -ENOSPC; // No space left
}



static int wfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    printf("mknod....\n");
    // Step 1: Ensure the file does not already exist
    struct wfs_inode *existing_inode = find_inode_by_path(path);
    if (existing_inode != NULL)
    {
        return -EEXIST; // File already exists
    }

    // Step 2: Find the parent directory inode
    char *parent_path = strdup(path);
    if (!parent_path)
    {
        return -ENOMEM; // Failed to allocate memory
    }

    char *last_slash = strrchr(parent_path, '/');
    if (!last_slash)
    {
        free(parent_path);
        return -ENOENT; // No parent directory path found
    }
    *last_slash = '\0'; // Terminate the parent path string before the last '/'

    struct wfs_inode *parent_inode = find_inode_by_path(parent_path);
    if (parent_inode == NULL)
    {
        free(parent_path);
        return -ENOENT; // Parent directory does not exist
    }
    if (!S_ISDIR(parent_inode->mode))
    {
        free(parent_path);
        return -ENOTDIR; // Parent is not a directory
    }

    // Step 3: Allocate a new inode for the new file
    int new_inode_num = allocate_inode();
    printf("new inode num is %d\n", new_inode_num);
    if (new_inode_num == -1)
    {
        free(parent_path);
        return -ENOSPC; // No space left to create a new inode
    }

    struct wfs_inode *new_inode = (struct wfs_inode *)((char *)mapped_memory + sb.i_blocks_ptr + new_inode_num * BLOCK_SIZE);
    new_inode->num = new_inode_num;
    new_inode->mode = mode;
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->size = 0;   // Initially empty
    new_inode->nlinks = 1; // One link to the file itself
    new_inode->atim = time(NULL);
    new_inode->mtim = time(NULL);
    new_inode->ctim = time(NULL);
    memset(new_inode->blocks, 0, sizeof(new_inode->blocks)); // Initialize all blocks to 0

    // Step 4: Add directory entry for the new file in the parent directory
    if (add_directory_entry(parent_inode, new_inode_num, last_slash + 1) != 0)
    {
        free(parent_path);
        // Optionally clear the inode bit if required
        // free_inode(new_inode_num);
        return -EIO; // Failed to add directory entry
    }

    free(parent_path);
    return 0;
}

static int wfs_mkdir(const char *path, mode_t mode)
{
    printf("mkdir....\n");
    // Step 1: Ensure the directory does not already exist
    struct wfs_inode *existing_inode = find_inode_by_path(path);
    if (existing_inode != NULL)
    {
        return -EEXIST; // Directory already exists
    }

    // Step 2: Find the parent directory inode
    char *parent_path = strdup(path);
    if (!parent_path)
    {
        return -ENOMEM; // Failed to allocate memory
    }

    char *last_slash = strrchr(parent_path, '/');
    if (!last_slash)
    {
        free(parent_path);
        return -ENOENT; // No parent directory path found
    }
    *last_slash = '\0'; // Terminate the parent path string before the last '/'

    struct wfs_inode *parent_inode = find_inode_by_path(parent_path);
    if (parent_inode == NULL)
    {
        free(parent_path);
        return -ENOENT; // Parent directory does not exist
    }
    if (!S_ISDIR(parent_inode->mode))
    {
        free(parent_path);
        return -ENOTDIR; // Parent is not a directory
    }

    // Step 3: Allocate a new inode for the new directory
    int new_inode_num = allocate_inode();
    printf("new inode num is %d\n", new_inode_num);
    if (new_inode_num == -1)
    {
        free(parent_path);
        return -ENOSPC; // No space left to create a new inode
    }

    struct wfs_inode *new_inode = (struct wfs_inode *)((char *)mapped_memory + sb.i_blocks_ptr + new_inode_num * BLOCK_SIZE);
    new_inode->num = new_inode_num;
    new_inode->mode = S_IFDIR | mode;
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->size = 0;   // Initially empty
    new_inode->nlinks = 2; // '.' and parent directory '..'
    new_inode->atim = time(NULL);
    new_inode->mtim = time(NULL);
    new_inode->ctim = time(NULL);
    memset(new_inode->blocks, 0, sizeof(new_inode->blocks)); // Initialize all blocks to 0

    // Step 4: Add directory entry for the new directory in the parent directory
    if (add_directory_entry(parent_inode, new_inode_num, last_slash + 1) != 0)
    {
        free(parent_path);
        return -EIO; // Failed to add directory entry
    }

    free(parent_path);
    return 0;
}

static int remove_directory_entry(struct wfs_inode *parent_inode, int inode_num, const char *entry_name)
{
    printf("removing directory entry %s%d\n", entry_name, inode_num);
    for (int i = 0; i < N_BLOCKS && parent_inode->blocks[i] != 0; i++)
    {
        struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + parent_inode->blocks[i]);
        for (int j = 0; j < (BLOCK_SIZE / sizeof(struct wfs_dentry)); j++)
        {
            printf("Inspecting entry %d: %s num:%d\n", j, dentries[j].name, dentries[j].num);
            if (dentries[j].num == inode_num && strcmp(dentries[j].name, entry_name) == 0)
            {
                printf("Entry found. Removing...\n");
                dentries[j].num = 0;                   // Mark the entry as free
                memset(dentries[j].name, 0, MAX_NAME); // Clear the name
                return 0;                              // Success
            }
        }
    }
    return -ENOENT; // Entry not found
}

static void free_inode(int inode_num)
{
    printf("freeing the inode\n");
    if (inode_num < 0 || inode_num >= sb.num_inodes)
    {
        return; // Out of bounds safety check
    }
    size_t byte_index = inode_num / 8;
    size_t bit_index = inode_num % 8;
    inode_bitmap[byte_index] &= ~(1 << bit_index); // Clear the bit
}

static void free_block(int block_num)
{
    printf("freeing the block\n");
    if (block_num < 0 || block_num >= sb.num_data_blocks)
    {
        return; // Out of bounds safety check
    }
    size_t byte_index = block_num / 8;
    size_t bit_index = block_num % 8;
    data_bitmap[byte_index] &= ~(1 << bit_index); // Clear the bit

    // Optionally clear the block data
    memset((char *)mapped_memory + block_num, 0, BLOCK_SIZE);
}

static int wfs_unlink(const char *path)
{
    printf("Unlinking file: %s\n", path);

    // Step 1: Locate the inode of the file
    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode)
    {
        return -ENOENT; // No such file
    }

    // Step 2: Ensure the file is not a directory
    if (S_ISDIR(inode->mode))
    {
        return -EISDIR; // Is a directory, not a file
    }

    // Step 3: Find the parent directory and the name of the file in the path
    char *parent_path = strdup(path);
    char *file_name = strrchr(parent_path, '/');
    if (file_name == NULL)
    {
        free(parent_path);
        return -ENOENT; // Path error
    }
    *file_name = '\0'; // Null-terminate the parent path
    file_name++;       // Move past the slash to the file name

    printf("Parent path: %s\n", parent_path);
    printf("File name: %s\n", file_name);

    struct wfs_inode *parent_inode = find_inode_by_path(parent_path);
    // free(parent_path);
    if (!parent_inode)
    {
        return -ENOENT; // Parent directory does not exist
    }
    printf("File name: %s\n", file_name);
    // Step 4: Remove the directory entry from the parent directory
    int result = remove_directory_entry(parent_inode, inode->num, file_name);
    if (result != 0)
    {
        return result; // Failed to remove directory entry
    }

    // Step 5: Free the inode and its blocks
    free_inode(inode->num);
    for (int i = 0; i < N_BLOCKS; i++)
    {
        if (inode->blocks[i] != 0)
        {
            free_block(inode->blocks[i]);
        }
    }

    return 0; // Success
}

static int wfs_rmdir(const char *path) {
    printf("Removing directory: %s\n", path);

    // Step 1: Locate the inode of the directory
    struct wfs_inode *dir_inode = find_inode_by_path(path);
    if (!dir_inode) {
        return -ENOENT;  // No such directory
    }

    // Step 2: Ensure the inode is a directory
    if (!S_ISDIR(dir_inode->mode)) {
        return -ENOTDIR;  // Not a directory
    }

    // Step 3: Check if directory is empty except for "." and ".."
    bool is_empty = true;
    for (int i = 0; i < N_BLOCKS && dir_inode->blocks[i] != 0; i++) {
        struct wfs_dentry *dentries = (struct wfs_dentry *)((char *)mapped_memory + dir_inode->blocks[i]);
        for (int j = 0; j < (BLOCK_SIZE / sizeof(struct wfs_dentry)); j++) {
            if (dentries[j].num != 0 && strcmp(dentries[j].name, ".") != 0 && strcmp(dentries[j].name, "..") != 0) {
                is_empty = false;
                break;
            }
        }
        if (!is_empty) break;
    }

    if (!is_empty) {
        return -ENOTEMPTY;  // Directory not empty
    }

    // Step 4: Find the parent directory and remove the directory entry
    char *parent_path = strdup(path);
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash == NULL || parent_path == last_slash) {
        free(parent_path);
        return -EIO;  // I/O error
    }
    *last_slash = '\0';
    struct wfs_inode *parent_inode = find_inode_by_path(parent_path);
    free(parent_path);

    if (!parent_inode) {
        return -ENOENT;  // Parent directory does not exist
    }

    // Step 5: Remove the directory entry
    char *dir_name = last_slash + 1;
    int ret = remove_directory_entry(parent_inode, dir_inode->num, dir_name);
    if (ret != 0) {
        return ret;  // Failed to remove directory entry
    }

    // Step 6: Free the inode and its blocks
    free_inode(dir_inode->num);
    for (int i = 0; i < N_BLOCKS; i++) {
        if (dir_inode->blocks[i] != 0) {
            free_block(dir_inode->blocks[i]);
        }
    }

    return 0; // Success
}
