#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int write_all_bytes(const char *buffer, ssize_t num_bytes) {
    ssize_t total_bytes_written = 0;
    while (total_bytes_written < num_bytes) {
        ssize_t bytes_written = write(STDOUT_FILENO, buffer + total_bytes_written,
                                      (size_t)(num_bytes - total_bytes_written));
        if (bytes_written == -1) {
            perror("Write Error");
            return -1;
        }
        // fprintf(stdout, "Wrote %zu bytes to stdout\n", bytes_written);
        total_bytes_written += bytes_written;
    }
    return 0;
}

int copy_fd_to_stdout(int fd) {
    char buffer[4096];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) != 0) {
        if (bytes_read == -1) {
            perror("Read Error");
            return -1;
        }
        if (write_all_bytes(buffer, bytes_read) == -1) {
            return -1;
        }
    }
    return 0;
}

int get_fd(const char *file_name) {
    int fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        perror("Error Opening File");
        return -1;
    }
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./jek_cat <file>\n");
        return 1;
    }

    int fd = get_fd(argv[1]);
    if (fd == -1) {
        return 1;
    }

    int cat_result = copy_fd_to_stdout(fd);

    if (close(fd) == -1) {
        fprintf(stderr, "Error Closing File %s\n", argv[1]);
        return 1;
    }

    if (cat_result == -1) {
        return 1;
    }

    return 0;
}
