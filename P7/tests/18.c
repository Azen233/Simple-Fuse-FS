#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "common/test.h"

const int file_block_num = 31;

int main() {
  int filesize = file_block_num * BLOCK_SIZE;
  int ret;
  int num_item = 31;

  char** filenames = (char**)calloc(num_item, sizeof(char*));
  for (size_t i = 0; i < num_item; i++) {
    filenames[i] = (char*)calloc(num_item, sizeof(char));
    sprintf(filenames[i], "file%ld", i);
  }

  char* buf = (char*)malloc(filesize);

  for (int i = 0; i < num_item; i++) {
    char* filename = (char*)calloc(32, sizeof(char));
    sprintf(filename, "mnt/%s", filenames[i]);
    CHECK(create_file(filename));
    int fd = ret;
    generate_random_data(buf, filesize);
    CHECK(write_file_check(fd, buf, filesize, filename, 0));
    CHECK(close_file(fd));
    MAP_DISK();
    int cur_file_num = i + 1;
    CHECK_INODE_AND_BLOCK_COUNT(
        1 + cur_file_num, expected_datablock_num(file_block_num, cur_file_num));
    UNMAP_DISK();
  }

  CHECK(read_dir_check("mnt", filenames, num_item));

  return PASS;
}