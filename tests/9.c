#include <stdio.h>
#include "common/test.h"

const int expected_inode_count = 2;
const int expected_data_block_count = 2;

int main() {
  int ret;
  CHECK(create_dir("mnt/data4"));

  CHECK(create_file("mnt/data4/data.txt"))
  int fd = ret;

  CHECK(close_file(fd));

  CHECK(remove_file("mnt/data4/data.txt"));

  MAP_DISK();

  CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count, expected_data_block_count);

  UNMAP_DISK();

  return PASS;
}
