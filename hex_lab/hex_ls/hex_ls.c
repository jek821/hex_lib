#include <dirent.h>
#include <getopt.h>
#include <stdio.h>

#define BLUE "\033[34m"
#define RESET "\033[0m"

int list_dir(const char *path, int show_all) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("jekls: cannot open directory");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;

        // Hide dotfiles (which includes "." and "..") unless -a was given.
        if (!show_all && name[0] == '.') {
            continue;
        }

        if (entry->d_type == DT_DIR) {
            printf(BLUE "%s" RESET "\n", name);
        } else {
            printf("%s\n", name);
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    int show_all = 0;
    int opt;

    while ((opt = getopt(argc, argv, "a")) != -1) {
        switch (opt) {
        case 'a':
            show_all = 1;
            break;
        case '?':
            fprintf(stderr, "Usage: ./jekls [-a] [directory]\n");
            return 1;
        }
    }

    // Optional non-flag argument selects the directory; default to ".".
    const char *path = (optind < argc) ? argv[optind] : ".";

    return list_dir(path, show_all) == -1 ? 1 : 0;
}
