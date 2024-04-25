#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "common/test.h"

const int expected_inode_count = 65;
const int expected_data_block_count = 4;

int main() {
  int ret;
  int num_item = 64;

  char** filenames = (char**)calloc(num_item, sizeof(char*));
  for (size_t i = 0; i < num_item; i++)
  {
    filenames[i] = (char*)calloc(32, sizeof(char));
    sprintf(filenames[i], "file%ld", i);
  }
  

  for (size_t i = 0; i < num_item; i++)
  {
    char* filename = (char*)calloc(num_item, sizeof(char));
    sprintf(filename, "mnt/%s", filenames[i]);
    CHECK(create_file(filename));
  }

  CHECK(read_dir_check("mnt", filenames, num_item));
  

  MAP_DISK();

  CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count, expected_data_block_count);

  UNMAP_DISK();

  return PASS;
}