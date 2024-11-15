#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "common/test.h"

const int file_block_num = D_BLOCK + 3;

const int expected_inode_count = 3;
const int expected_data_block_count = 37;

int main() {
  int filesize = file_block_num * BLOCK_SIZE;
  char* buf1 = (char*)malloc(filesize);
  generate_random_data(buf1, filesize);
  char* buf2 = (char*)malloc(filesize);
  generate_random_data(buf2, filesize);

  int ret;
  CHECK(create_file("mnt/large1.txt"));
  int fd1 = ret;

  CHECK(create_file("mnt/large2.txt"));
  int fd2 = ret;

  for (size_t i = 0; i < filesize; i += BLOCK_SIZE) {
    CHECK(write_file_check(fd1, buf1 + i, filesize, "mnt/large1.txt", i));
    CHECK(write_file_check(fd2, buf2 + i, filesize, "mnt/large2.txt", i));
  }

  CHECK(close_file(fd1));
  CHECK(close_file(fd2));

  CHECK(open_file_read("mnt/large1.txt"));
  fd1 = ret;
  CHECK(read_file_check(fd1, buf1, filesize, "mnt/large1.txt", 0));
  CHECK(close_file(fd1));
  free(buf1);

  CHECK(open_file_read("mnt/large2.txt"));
  fd2 = ret;
  CHECK(read_file_check(fd2, buf2, filesize, "mnt/large2.txt", 0));
  CHECK(close_file(fd2));
  free(buf2);

  MAP_DISK();

  CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count, expected_data_block_count);

  UNMAP_DISK();

  return PASS;
}