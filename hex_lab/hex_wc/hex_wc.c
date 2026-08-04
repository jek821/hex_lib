#include "../../hex_lib/hex_debug/hex_debug.h"
#include <fcntl.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Which counts the user asked for (set from the command-line flags).
typedef struct {
    int show_bytes;
    int show_words;
    int show_lines;
} Flags;

// Running totals accumulated across the file.
typedef struct {
    size_t byte_count;
    long word_count;
    long line_count;
} File_Data;

void usage_error(void) {
    fprintf(stderr, "==================================================\n");
    fprintf(stderr, "USAGE ERROR:\nCorrect Usage: ./jekwc <flags> <file>\n");
    fprintf(stderr, "Flags:\n-b (print number of bytes)\n-w (print number of words)\n-l "
                    "(print number of lines)\nNo flags results in all values being "
                    "printed\n");
    fprintf(stderr, "==================================================\n");
}

int handle_flags(const int *argc, char *argv[], Flags *flags) {
    int opt;

    while ((opt = getopt(*argc, argv, "bwl")) != -1) {
        switch (opt) {
        case 'b':
            flags->show_bytes = 1;
            break;
        case 'w':
            flags->show_words = 1;
            break;
        case 'l':
            flags->show_lines = 1;
            break;
        case '?':
            usage_error();
            return -1;
        }
    }
    // No flags supplied: default to printing all three values.
    if (!flags->show_bytes && !flags->show_words && !flags->show_lines) {
        flags->show_words = 1;
        flags->show_bytes = 1;
        flags->show_lines = 1;
    }
    return 0;
}

Flags *initialize_flags(void) {
    Flags *flags = (Flags *)malloc(sizeof(Flags));
    flags->show_bytes = 0;
    flags->show_lines = 0;
    flags->show_words = 0;
    return flags;
}

int get_fd(char *file_name) {
    int fd;

    if ((fd = open(file_name, O_RDONLY)) == -1) {
        perror("Error Opening File");
        return -1;
    }
    return fd;
}

long lines_from_buff(char *buffer, ssize_t bytes_to_read) {
    long line_count = 0;

    for (ssize_t i = 0; i < bytes_to_read; i++) {
        if (buffer[i] == '\n') {
            line_count++;
        }
    }

    return line_count;
}

int is_space(char byte) {
    int result = (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\v' || byte == '\f' ||
                  byte == '\r');
    debug("is space: %d", result);
    return result;
}

long words_from_buff(char *buffer, ssize_t bytes_to_read, int *in_word) {
    debug("entered words from buff");
    long word_count = 0;

    for (ssize_t i = 0; i < bytes_to_read; i++) {
        debug("Iteration %zu in words from buff loop", i);
        if (is_space(buffer[i])) {
            *in_word = 0;
        } else {
            if (!*in_word) {
                word_count++;
            }
            *in_word = 1;
        }
    }
    return word_count;
}

int get_data(int fd, File_Data *file_data, Flags *flags) {
    char byte_buffer[4096];
    ssize_t bytes_read;
    int in_word = 0;
    while ((bytes_read = read(fd, byte_buffer, sizeof(byte_buffer))) != 0) {
        debug("entering get data while loop");

        if (bytes_read == -1) {
            perror("Read Error");
            return -1;
        }

        if (flags->show_bytes) {
            file_data->byte_count += (size_t)bytes_read;
        }
        if (flags->show_lines) {
            file_data->line_count += lines_from_buff(byte_buffer, bytes_read);
        }
        if (flags->show_words) {
            file_data->word_count += words_from_buff(byte_buffer, bytes_read, &in_word);
        }
    }
    return 0;
}

void print_data(Flags *flags, File_Data *file_data) {
    if (flags->show_bytes) {
        fprintf(stdout, "Bytes: %zu\n", file_data->byte_count);
    }
    if (flags->show_lines) {
        fprintf(stdout, "Lines: %ld\n", file_data->line_count);
    }
    if (flags->show_words) {
        fprintf(stdout, "Words: %ld\n", file_data->word_count);
    }
}

int main(int argc, char *argv[]) {
    Flags *flags = initialize_flags();
    File_Data *file_data = (File_Data *)calloc(1, sizeof(File_Data));

    // Handle flags
    if (handle_flags(&argc, argv, flags) == -1) {
        free(flags);
        free(file_data);
        return 1;
    }

    debug("argc: %d", argc);
    debug("optind: %d", optind);

    // A file argument must follow the flags.
    if (optind >= argc) {
        usage_error();
        free(flags);
        free(file_data);
        return 1;
    }

    // Handle file (get file descriptor)
    int fd = get_fd(argv[optind]);
    debug("File Descriptor: %d", fd);
    if (fd == -1) {
        free(flags);
        free(file_data);
        return 1;
    }

    // Collect data from file (fill File_Data struct)
    int data_result = get_data(fd, file_data, flags);

    // Send file data to STDOUT
    if (data_result != -1) {
        print_data(flags, file_data);
    }

    close(fd);
    free(flags);
    free(file_data);
    return data_result == -1 ? 1 : 0;
}