#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "common/test.h"

const int expected_inode_count = 96;
const int expected_data_block_count = 6;

int main() {
  int ret;
  int num_item = 95;

  char** filenames = (char**)calloc(num_item, sizeof(char*));
  for (size_t i = 0; i < num_item; i++) {
    filenames[i] = (char*)calloc(32, sizeof(char));
    sprintf(filenames[i], "file%ld", i);
  }

  for (size_t i = 0; i < num_item; i++) {
    char* filename = (char*)calloc(32, sizeof(char));
    sprintf(filename, "mnt/%s", filenames[i]);
    CHECK(create_file(filename));
  }

  char* filename = (char*)calloc(32, sizeof(char));
  sprintf(filename, "mnt/%d", num_item);
  int rc = open(filename, O_CREAT | O_WRONLY, 0666);
  if (rc >= 0) {
    printf(
        "Expected to fail to create file because of running out of inodes: "
        "%s\n",
        filename);
    return FAIL;
  } else if (errno != ENOSPC) {
    printf("Expected errno to be ENOSPC, but got %d\n", errno);
    return FAIL;
  }

  CHECK(read_dir_check("mnt", filenames, num_item));

  MAP_DISK();

  CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count, expected_data_block_count);

  UNMAP_DISK();

  return PASS;
}