#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int pipe_max = 0;
static int pipe_amount = 0;

int tdo_mock_pipe(int fds[2]) {
    if (pipe_amount++ < pipe_max) return pipe(fds);
    return 1;
}

int tdo_runner_main(int argc, char **argv);
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to pipe\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-pipe-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-pipe-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    pipe_max = atoi(argv[argc - 1]);
    if (pipe_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to pipe, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
