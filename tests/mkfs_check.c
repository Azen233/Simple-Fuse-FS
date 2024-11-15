#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "common/test.h"

#define TEST_CHECK(EXPECT, FOUND, ERR_MSG)                                         \
 if ((FOUND) != (EXPECT)) {                                                        \
    printf(ERR_MSG ": expected %ld (%#lx), found %ld (%#lx)\n", (long)(EXPECT), (long)(EXPECT), (long)(FOUND), (long)(FOUND));                \
    return FAIL;                                                                   \
  }                                                                                \

int roundup(int num, int factor) {
    return num % factor == 0 ? num : num + (factor - (num % factor));
}

int main(int argc, char** argv) {
  char* diskimg;
  int inodes, blocks;
  int opt;
  
  while ((opt = getopt(argc, argv, "d:i:b:")) != -1) {
      switch (opt) {
      case 'd':
          diskimg = optarg;
          break;
      case 'i':
          inodes = atoi(optarg);
          break;
      case 'b':
          blocks = atoi(optarg);
          break;
      default:
          // printf("usage: ./mkfs -d <disk img> -i <num inodes> -b <num data blocks>\n");
          exit(1);
      }
  }

  inodes = roundup(inodes, 32);
  blocks = roundup(blocks, 32);

  int fd = open(diskimg, O_RDWR);
  if (fd < 0) {
    perror("open");
    return INTERNAL_ERR;
  }

  struct stat statbuf;
  if (fstat(fd, &statbuf) != 0) {
    perror("fstat");
    return INTERNAL_ERR;
  }

  char* map = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);

  struct wfs_sb* sb = (struct wfs_sb*)map;

  TEST_CHECK(inodes, sb->num_inodes, "Wrong inode number");
  TEST_CHECK(blocks, sb->num_data_blocks, "Wrong data block number");
  off_t i_bitmap_ptr = sizeof(struct wfs_sb);
  TEST_CHECK(i_bitmap_ptr, sb->i_bitmap_ptr, "Wrong i_bitmap_ptr in super block");
  off_t d_bitmap_ptr = i_bitmap_ptr + (inodes / 8);
  TEST_CHECK(d_bitmap_ptr, sb->d_bitmap_ptr, "Wrong d_bitmap_ptr in super block");
  off_t i_blocks_ptr = d_bitmap_ptr + (blocks / 8);
  TEST_CHECK(i_blocks_ptr, sb->i_blocks_ptr, "Wrong i_blocks_ptr in super block");
  off_t d_blocks_ptr = i_blocks_ptr + (inodes * BLOCK_SIZE);
  TEST_CHECK(d_blocks_ptr, sb->d_blocks_ptr, "Wrong d_blocks_ptr in super block");

  munmap(map, statbuf.st_size);
  return PASS;
}