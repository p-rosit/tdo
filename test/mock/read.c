#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int read_max = 0;
static int read_amount = 0;

ssize_t tdo_mock_read(int fd, char *buffer, size_t size) {
    if (read_amount++ < read_max) {
        return read(fd, buffer, size);
    }
    errno = EBADF;
    return -1;
}

int tdo_runner_main(int argc, char **argv);
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to read\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-read-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-read-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    read_max = atoi(argv[argc - 1]);
    if (read_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to read, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv + 2);
}
