#include <math.h>
#include <sys/types.h>
#include "wfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

size_t align_to_block(size_t size)
{
    if (size % BLOCK_SIZE == 0)
    {
        return size;
    }
    else
    {
        return (size / BLOCK_SIZE + 1) * BLOCK_SIZE;
    }
}

size_t calculate_bitmap_size(size_t num_items)
{
    return (num_items + 7) / 8; // Add 7 to ensure rounding up when dividing by 8
}

int main(int argc, char *argv[])
{
    char *disk_path = NULL;
    int num_inodes = 0;
    int num_data_blocks = 0;

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

    // In your main or appropriate function
    size_t inode_bitmap_size = calculate_bitmap_size(num_inodes);
    size_t data_bitmap_size = calculate_bitmap_size(num_data_blocks);

    // Aligning sections to block boundaries
    inode_bitmap_size = align_to_block(inode_bitmap_size);
    data_bitmap_size = align_to_block(data_bitmap_size);

    size_t inodes_size = num_inodes * sizeof(struct wfs_inode);
    // size_t data_blocks_size = num_data_blocks * BLOCK_SIZE;

    // Calculate positions
    off_t i_bitmap_ptr = sizeof(struct wfs_sb);
    off_t d_bitmap_ptr = i_bitmap_ptr + inode_bitmap_size;
    off_t i_blocks_ptr = d_bitmap_ptr + data_bitmap_size;
    off_t d_blocks_ptr = i_blocks_ptr + inodes_size;

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
