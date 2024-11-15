#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "common/test.h"

const int file_block_num = 1;

int main() {
  int filesize = file_block_num * BLOCK_SIZE;
  int ret;
  int item_num = 64;

  char** filenames = (char**)calloc(item_num, sizeof(char*));
  for (size_t i = 0; i < item_num; i++) {
    filenames[i] = (char*)calloc(item_num, sizeof(char));
    sprintf(filenames[i], "file%ld", i);
  }

  CHECK(create_file("mnt/dummy"));
  CHECK(create_dir("mnt/subdir"));

  char* buf = (char*)malloc(filesize);

  printf("Creating %d files\n", item_num);

  for (int i = 0; i < item_num; i++) {
    char* filename = (char*)calloc(32, sizeof(char));
    sprintf(filename, "mnt/subdir/%s", filenames[i]);
    CHECK(create_file(filename));
    int fd = ret;
    generate_random_data(buf, filesize);
    CHECK(write_file_check(fd, buf, filesize, filename, 0));
    CHECK(close_file(fd));
  }

  printf("Checking directory\n");

  CHECK(read_dir_check("mnt/subdir", filenames, item_num));

  printf("Removing %d files\n", item_num);

  for (int i = 0; i < item_num; i++) {
    char* filename = (char*)calloc(32, sizeof(char));
    sprintf(filename, "mnt/subdir/%s", filenames[i]);
    CHECK(remove_file(filename));
  }

  printf("Checking directory\n");

  CHECK(read_dir_check("mnt/subdir", NULL, 0));

  CHECK(remove_dir("mnt/subdir"));

  MAP_DISK();
  CHECK_INODE_AND_BLOCK_COUNT(2, 1);  // Note that the root directory is not
                                      // empty: file "dummy" is still there
  UNMAP_DISK();

  return PASS;
}