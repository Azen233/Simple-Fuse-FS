#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "common/test.h"

const int block_num = 64;

int main() {
  int filesize =
      (block_num - 2) *
      BLOCK_SIZE;  // 2 blocks are used for root dir and indirect block
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
  CHECK_INODE_AND_BLOCK_COUNT(2, 64);
  UNMAP_DISK();

  CHECK(open_file_write("mnt/large.txt"));
  fd = ret;

  lseek(fd, 0, SEEK_END);
  int rc = write(fd, "a", 1);
  if (rc >= 0) {
    printf(
        "Expected to fail to create file because of running out of inodes: "
        "%s\n",
        "mnt/large.txt");
    return FAIL;
  } else if (errno != ENOSPC) {
    printf("Expected errno to be ENOSPC, but got %d\n", errno);
    return FAIL;
  }

  return PASS;
}