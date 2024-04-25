#include <sys/types.h>
#include "wfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int roundup(int num, int factor)
{
    return num % factor == 0 ? num : num + (factor - (num % factor));
}


int main(int argc, char *argv[])
{
    char *disk_path = NULL;
    int num_inodes = 0;
    int num_data_blocks = 0;
    int inode_bitmap_size = 0;
    int data_bitmap_size = 0;

    // Parse command line arguments
    if (argc != 7)
    {
        fprintf(stderr, "Usage: %s -d disk_img -i num_inodes -b num_data_blocks\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0)
        {
            disk_path = argv[++i];
        }
        else if (strcmp(argv[i], "-i") == 0)
        {
            num_inodes = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            num_data_blocks = atoi(argv[++i]);
        }
    }

    if (!disk_path || num_inodes <= 0 || num_data_blocks <= 0)
    {
        fprintf(stderr, "Invalid arguments\n");
        exit(EXIT_FAILURE);
    }
    num_inodes = roundup(num_inodes, 32);
    num_data_blocks = roundup(num_data_blocks, 32);

    off_t i_bitmap_ptr = sizeof(struct wfs_sb);
    off_t d_bitmap_ptr = i_bitmap_ptr + (num_inodes / 8);
    off_t i_blocks_ptr = d_bitmap_ptr + (num_data_blocks / 8);
    off_t d_blocks_ptr = i_blocks_ptr + (num_inodes * BLOCK_SIZE);

    int fd = open(disk_path, O_RDWR | O_CREAT, 0644);
    if (fd == -1)
    {
        perror("Failed to open disk image");
        exit(EXIT_FAILURE);
    }

    // Initialize the superblock
    struct wfs_sb sb = {
        .num_inodes = num_inodes,
        .num_data_blocks = num_data_blocks,
        .i_bitmap_ptr = i_bitmap_ptr,
        .d_bitmap_ptr = d_bitmap_ptr,
        .i_blocks_ptr = i_blocks_ptr,
        .d_blocks_ptr = d_blocks_ptr};
    // Write the superblock to the disk image
    if (write(fd, &sb, sizeof(sb)) != sizeof(sb))
    {
        perror("Failed to write superblock");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Zero out inode bitmap
    char *zero_buffer = calloc(1, inode_bitmap_size);
    if (!zero_buffer)
    {
        perror("Memory allocation failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (lseek(fd, i_bitmap_ptr, SEEK_SET) == -1 ||
        write(fd, zero_buffer, inode_bitmap_size) != inode_bitmap_size)
    {
        perror("Failed to write inode bitmap");
        free(zero_buffer);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Zero out data block bitmap
    if (lseek(fd, d_bitmap_ptr, SEEK_SET) == -1 ||
        write(fd, zero_buffer, data_bitmap_size) != data_bitmap_size)
    {
        perror("Failed to write data block bitmap");
        free(zero_buffer);
        close(fd);
        exit(EXIT_FAILURE);
    }
    free(zero_buffer);

    // Initialize root inode
    struct wfs_inode root_inode = {
        .num = 0,
        .mode = S_IFDIR | 0755, // Directory with rwxr-xr-x permissions
        .uid = getuid(),        // Owner's user ID
        .gid = getgid(),        // Owner's group ID
        .size = 0,              // Initially empty
        .nlinks = 2,            // Standard for directories (self and parent)

        .atim = time(NULL),
        .mtim = time(NULL),
        .ctim = time(NULL),

        .blocks = {0} // Initialize all block pointers to 0
    };

    if (lseek(fd, i_blocks_ptr, SEEK_SET) == -1 ||
        write(fd, &root_inode, sizeof(root_inode)) != sizeof(root_inode))
    {
        perror("Failed to write root inode");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Finish up and close file descriptor
    close(fd);
}
