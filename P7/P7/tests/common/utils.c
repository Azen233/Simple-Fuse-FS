#include "test.h"

const char* new_disk_path = "disk.img";

char* map_disk() {
  int fd = open(new_disk_path, O_RDWR);
  if (fd < 0) {
    perror("open");
    return NULL;
  }

  struct stat statbuf;
  if (fstat(fd, &statbuf) != 0) {
    perror("fstat");
    return NULL;
  }

  char* map = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  return map;
}

char* unmap_disk(char* disk_map) {
  int fd = open(new_disk_path, O_RDWR);
  if (fd < 0) {
    perror("open");
    return NULL;
  }

  struct stat statbuf;
  if (fstat(fd, &statbuf) != 0) {
    perror("fstat");
    return NULL;
  }
  if (munmap(disk_map, statbuf.st_size) != 0) {
    perror("munmap");
    return NULL;
  }
  return disk_map;
}

size_t bitmap_count(const char* start, size_t size) {
  size_t count = 0;
  for (size_t i = 0; i < size; i++) {
    for (int j = 0; j < 8; j++) {
      if ((start[i] & (1 << j)) != 0) {
        count++;
      }
    }
  }
  return count;
}

size_t inode_count(char* disk_map) {
  struct wfs_sb* sb = (struct wfs_sb*)disk_map;
  char* i_bitmap = disk_map + sb->i_bitmap_ptr;
  return bitmap_count(i_bitmap, sb->num_inodes / 8);
}

size_t data_block_count(char* disk_map) {
  struct wfs_sb* sb = (struct wfs_sb*)disk_map;
  char* d_bitmap = disk_map + sb->d_bitmap_ptr;
  return bitmap_count(d_bitmap, sb->num_data_blocks / 8);
}

int open_file_read(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    printf("Unable to open file: %s\n", path);
    perror("open");
    return FAIL;
  }
  return fd;
}

int open_file_write(const char* path) {
  int fd = open(path, O_WRONLY);
  if (fd == -1) {
    printf("Unable to open file: %s\n", path);
    perror("open");
    return FAIL;
  }
  return fd;
}

int read_file_check(int fd, const char* expected_content, size_t content_size,
                    const char* path, off_t offset) {
  char buffer[content_size + 1];
  lseek(fd, offset, SEEK_SET);
  ssize_t bytesRead = read(fd, buffer, content_size);
  if (bytesRead != content_size) {
    close(fd);
    printf(
        "Wrong number of bytes read from file: %s. Expected %lu bytes, read "
        "%ld bytes\n",
        path, content_size, bytesRead);
    return FAIL;
  }
  for (size_t i = 0; i < content_size; i++) {
    if (buffer[i] != expected_content[i]) {
      close(fd);
      printf("First mismatch at byte %lu: expected %x != found %x\n", i,
             expected_content[i], buffer[i]);
      return FAIL;
    }
  }

  return PASS;
}

int write_file_check(int fd, const char* content, size_t content_size,
                     const char* path, off_t offset) {
  lseek(fd, offset, SEEK_SET);
  ssize_t bytesWritten = write(fd, content, content_size);
  if (bytesWritten != content_size) {
    close(fd);
    printf(
        "Unable to write to file: %s. Expected to write %lu bytes, have "
        "written %lu bytes\n",
        path, content_size, bytesWritten);
    perror("write");
    return FAIL;
  }
  return PASS;
}

int close_file(int fd) {
  if (close(fd) == -1) {
    perror("close");
    return FAIL;
  }
  return PASS;
}

int create_file(const char* path) {
  int fd = open(path, O_CREAT | O_WRONLY, 0666);
  if (fd == -1) {
    printf("Unable to create file: %s\n", path);
    perror("open");
    return FAIL;
  }
  return fd;
}

int create_dir(const char* path) {
  if (mkdir(path, 0777) == -1) {
    printf("Unable to create directory: %s\n", path);
    perror("mkdir");
    return FAIL;
  }
  return PASS;
}

int remove_file(const char* path) {
  if (unlink(path) == -1) {
    printf("Unable to remove file: %s\n", path);
    perror("unlink");
    return FAIL;
  }
  return PASS;
}

int remove_dir(const char* path) {
  if (rmdir(path) == -1) {
    printf("Unable to remove directory: %s\n", path);
    perror("rmdir");
    return FAIL;
  }
  return PASS;
}

int read_dir_check(const char* path, char* const* expected_dentries,
                   size_t num_expected_dentries) {
  DIR* dir = opendir(path);
  if (dir == NULL) {
    printf("Unable to open directory: %s\n", path);
    perror("opendir");
    return FAIL;
  }

  int* found_num = (int*)calloc(num_expected_dentries, sizeof(int));

  struct dirent* entry;
  int ret = PASS;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    int found = 0;
    for (size_t i = 0; i < num_expected_dentries; i++) {
      if (strcmp(entry->d_name, expected_dentries[i]) == 0) {
        found_num[i] += 1;
        found = 1;
        break;
      }
    }
    if (!found) {
      printf("Unexpected directory entry: %s\n", entry->d_name);
      ret = FAIL;
    }
  }

  for (size_t i = 0; i < num_expected_dentries; i++) {
    if (found_num[i] == 0) {
      printf("Expected directory entry not found: %s\n", expected_dentries[i]);
      ret = FAIL;
    }
    if (found_num[i] > 1) {
      printf("Duplicate directory entry found: %s\n", expected_dentries[i]);
      ret = FAIL;
    }
  }

  closedir(dir);
  free(found_num);
  return ret;
}

void generate_random_data(char* buf, size_t size) {
  for (size_t i = 0; i < size; i++) {
    buf[i] = rand() % 256;
  }
}

int ceil_div(int a, int b) { return (a + b - 1) / b; }

// Suppose all files are located in the root directory and each file has the
// same number of blocks.
int expected_datablock_num(int file_block_num, int cur_file_num) {
  int indirect_block_num = (int)(file_block_num > IND_BLOCK);
  return ceil_div(cur_file_num, BLOCK_SIZE / sizeof(struct wfs_dentry)) +
         (file_block_num + indirect_block_num) * cur_file_num;
}
