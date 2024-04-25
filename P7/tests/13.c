#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "common/test.h"

const int expected_inode_count = 2;
const int expected_data_block_count = 5;

int main() {
  int filesize = 4 * BLOCK_SIZE;
  char* buf = (char*)malloc(filesize);
  generate_random_data(buf, filesize);

  int ret;
  CHECK(create_file("mnt/file.txt"));
  int fd = ret;

  CHECK(write_file_check(fd, buf, filesize, "mnt/file.txt", 0));
  CHECK(close_file(fd));

  CHECK(open_file_read("mnt/file.txt"));
  fd = ret;
  CHECK(read_file_check(fd, buf, filesize, "mnt/file.txt", 0));
  CHECK(close_file(fd));

  CHECK(open_file_write("mnt/file.txt"));
  fd = ret;
  char* buf_overwrite = (char*)malloc(BLOCK_SIZE);
  generate_random_data(buf_overwrite, BLOCK_SIZE);
  ssize_t offset = BLOCK_SIZE / 2;
  CHECK(
      write_file_check(fd, buf_overwrite, BLOCK_SIZE, "mnt/file.txt", offset));
  CHECK(close_file(fd));

  CHECK(open_file_read("mnt/file.txt"));
  fd = ret;
  memcpy(buf + offset, buf_overwrite, BLOCK_SIZE);
  CHECK(read_file_check(fd, buf, filesize, "mnt/file.txt", 0));
  CHECK(close_file(fd));

  free(buf_overwrite);
  free(buf);

  MAP_DISK();

  CHECK_INODE_AND_BLOCK_COUNT(expected_inode_count, expected_data_block_count);

  UNMAP_DISK();

  return PASS;
}