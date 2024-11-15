#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>

#define PASS 0
#define FAIL -1
#define INTERNAL_ERR 1

#define BLOCK_SIZE (512)
#define MAX_NAME (28)

#define D_BLOCK (6)
#define IND_BLOCK (D_BLOCK + 1)
#define N_BLOCKS (IND_BLOCK + 1)

/*
  The fields in the superblock should reflect the structure of the filesystem.
  `mkfs` writes the superblock to offset 0 of the disk image.
  The disk image will have this format:

          d_bitmap_ptr       d_blocks_ptr
               v                  v
+----+---------+---------+--------+--------------------------+
| SB | IBITMAP | DBITMAP | INODES |       DATA BLOCKS        |
+----+---------+---------+--------+--------------------------+
0    ^                   ^
i_bitmap_ptr        i_blocks_ptr

*/

// Superblock
struct wfs_sb {
  size_t num_inodes;
  size_t num_data_blocks;
  off_t i_bitmap_ptr;
  off_t d_bitmap_ptr;
  off_t i_blocks_ptr;
  off_t d_blocks_ptr;
};

// Inode
struct wfs_inode {
  int num;     /* Inode number */
  mode_t mode; /* File type and mode */
  uid_t uid;   /* User ID of owner */
  gid_t gid;   /* Group ID of owner */
  off_t size;  /* Total size, in bytes */
  int nlinks;  /* Number of links */

  time_t atim; /* Time of last access */
  time_t mtim; /* Time of last modification */
  time_t ctim; /* Time of last status change */

  off_t blocks[N_BLOCKS];
};

// Directory entry
struct wfs_dentry {
  char name[MAX_NAME];
  int num;
};

extern const char* new_disk_path;

char* map_disk();
char* unmap_disk(char* disk_map);
size_t inode_count(char* disk_map);
size_t data_block_count(char* disk_map);
int open_file_read(const char* path);
int open_file_write(const char* path);
int read_file_check(int fd, const char* expected_content, size_t content_size,
                    const char* path, off_t offset);
int write_file_check(int fd, const char* content, size_t content_size,
                     const char* path, off_t offset);
int close_file(int fd);
int create_file(const char* path);
int create_dir(const char* path);
int remove_file(const char* path);
int remove_dir(const char* path);
int read_dir_check(const char* path, char* const* expected_dentries,
                   size_t num_expected_dentries);
void generate_random_data(char* buf, size_t size);
int ceil_div(int a, int b);
int expected_datablock_num(int file_block_num, int cur_file_num);

#define MAP_DISK()                        \
  char* disk_map = map_disk();            \
  if (disk_map == NULL) {                 \
    printf("Failed to map disk image\n"); \
    return FAIL;                          \
  }

#define UNMAP_DISK()                        \
  disk_map = unmap_disk(disk_map);          \
  if (disk_map == NULL) {                   \
    printf("Failed to unmap disk image\n"); \
    return FAIL;                            \
  }

#define CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count,          \
                                    expected_data_block_count)     \
  if (inode_count(disk_map) != expected_inode_count) {             \
    printf("Wrong inode count: expected %d, found %ld\n",          \
           expected_inode_count, inode_count(disk_map));           \
    UNMAP_DISK();                                                  \
    return FAIL;                                                   \
  }                                                                \
                                                                   \
  if (data_block_count(disk_map) != expected_data_block_count) {   \
    printf("Wrong data block count: expected %d, found %ld\n",     \
           expected_data_block_count, data_block_count(disk_map)); \
    UNMAP_DISK();                                                  \
    return FAIL;                                                   \
  }

#define CHECK_MAPPED(cond) \
  ret = cond;              \
  if (ret == FAIL) {       \
    UNMAP_DISK();          \
    return FAIL;           \
  }

#define CHECK(cond)  \
  ret = cond;        \
  if (ret == FAIL) { \
    return FAIL;     \
  }

#endif