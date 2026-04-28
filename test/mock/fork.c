#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int fork_max = 0;
static int fork_amount = 0;

int tdo_mock_fork(void) {
    if (fork_amount++ < fork_max) {
        return fork();
    }
    errno = ENOMEM;
    return -1;
}

int tdo_runner_main(int argc, char **argv);
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to fork\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-fork-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-fork-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    fork_max = atoi(argv[argc - 1]);
    if (fork_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to fork, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
