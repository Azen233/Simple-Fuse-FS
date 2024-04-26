#include <stdio.h>
#include <string.h>
#include "common/test.h"

const int expected_inode_count = 2;
const int expected_data_block_count = 2;

int main() {
  int ret;
  CHECK(create_file("mnt/data11.txt"));
  int fd = ret;

  CHECK(write_file_check(fd, "Hello", 5, "mnt/data11.txt", 0));

  CHECK(close_file(fd));

  CHECK(open_file_read("mnt/data11.txt"));
  fd = ret;

  CHECK(read_file_check(fd, "Hello", 5, "mnt/data11.txt", 0));

  CHECK(close_file(fd));

  MAP_DISK();

  CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count, expected_data_block_count);

  UNMAP_DISK();

  return PASS;
}