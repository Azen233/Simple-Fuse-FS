#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define SUCCESS 0
#define FAIL -1

/**
 * Creates a file and writes content to it in the specified directory.
 * 
 * @param path The path to the file to be created, including directory.
 * @param content The content to write to the file.
 * @return SUCCESS (0) on success, FAIL (-1) on error.
 */
int create_and_write_file(const char* path, const char* content) {
    // Create or open the file with write access, set permissions to 0644
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        printf("Unable to create or open file: %s\n", path);
        perror("Error on open");
        return FAIL;
    }

    // Write content to the file
    size_t length = strlen(content);
    if (write(fd, content, length) != length) {
        printf("Failed to write to file: %s\n", path);
        perror("Error on write");
        close(fd); // Attempt to close the file descriptor before returning
        return FAIL;
    }

    // Close the file descriptor
    if (close(fd) == -1) {
        printf("Failed to close file: %s\n", path);
        perror("Error on close");
        return FAIL;
    }

    return SUCCESS;
}

int main() {
    const char* file_path = "/mnt/test.txt"; // Adjusted path to point to /mnt directory
    const char* content = "hello world";

    // Call the function to create a file and write content to it
    if (create_and_write_file(file_path, content) == FAIL) {
        return 1; // Return non-zero to indicate error
    }

    printf("File '%s' created and written successfully.\n", file_path);
    return 0; // Return zero to indicate success
}
