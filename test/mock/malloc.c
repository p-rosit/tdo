#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int malloc_max = 0;
static int malloc_amount = 0;

void *tdo_mock_malloc(size_t size) {
    if (malloc_amount++ < malloc_max) {
        return malloc(size);
    }
    return NULL;
}

int tdo_runner_main(int argc, char **argv);
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to malloc\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-malloc-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-malloc-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    malloc_max = atoi(argv[argc - 1]);
    if (malloc_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to malloc, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
