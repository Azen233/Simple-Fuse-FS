#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "common/test.h"

int main() {
  const char* paths[] = {"mnt/file0",       "mnt/file1",
                         "mnt/dir0/file00", "mnt/dir0/file01",
                         "mnt/dir1/file10", "mnt/dir1/file11"};

  for (size_t i = 0; i < 6; i++) {
    int ret;
    CHECK(open_file_read(paths[i]));
    int fd = ret;

    printf("fd: %d\n", fd);

    const char* content = "content";
    CHECK(read_file_check(fd, content, strlen(content), paths[i], 0));
    
    CHECK(close_file(fd));
  }

  return PASS;
}
