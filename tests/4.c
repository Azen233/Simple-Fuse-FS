#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common/test.h"

const int expected_inode_count = 3;
const int expected_data_block_count = 2;

int main() {
  int ret;
  CHECK(create_dir("mnt/data2"));

  CHECK(create_dir("mnt/data2/data3"));

  MAP_DISK();

  CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count, expected_data_block_count);

  UNMAP_DISK();

  return PASS;
}
