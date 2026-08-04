#include "hex_debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int debug_enabled = 0;

void hex_debug_init(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hex_debug") == 0) {
            debug_enabled = 1;
            return;
        }
    }
}

void debug(const char *format, ...) {
    if (!debug_enabled) {
        return;
    }

    va_list arguments;

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
}