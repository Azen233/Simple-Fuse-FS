#include <stdio.h>
#include "common/test.h"

const int expected_inode_count = 1;
const int expected_data_block_count = 1;

int main() {
  int ret;
  CHECK(create_file("mnt/file0"));
  int fd = ret;
  CHECK(close_file(fd));
  
  CHECK(remove_file("mnt/file0"));

  MAP_DISK();

  CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count, expected_data_block_count);

  UNMAP_DISK();

  return PASS;
}
