#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "common/test.h"

const int file_block_num = D_BLOCK + 3;

const int expected_inode_count = 2;
const int expected_data_block_count = 1 + file_block_num + 1;

int main() {
  int filesize = file_block_num * BLOCK_SIZE;
  char* buf = (char*)malloc(filesize);
  generate_random_data(buf, filesize);

  int ret;
  CHECK(create_file("mnt/large.txt"));
  int fd = ret;

  CHECK(write_file_check(fd, buf, filesize, "mnt/large.txt", 0));

  CHECK(close_file(fd));

  CHECK(open_file_read("mnt/large.txt"));
  fd = ret;

  CHECK(read_file_check(fd, buf, filesize, "mnt/large.txt", 0));

  free(buf);
  CHECK(close_file(fd));

  MAP_DISK();

  CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count, expected_data_block_count);

  UNMAP_DISK();

  return PASS;
}