#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int poll_max = 0;
static int poll_amount = 0;

int tdo_mock_poll(struct pollfd *fds, nfds_t count, int timeout) {
    if (poll_amount++ < poll_max) return poll(fds, count, timeout);
    return 1;
}

int tdo_runner_main(int argc, char **argv);
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to poll\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-poll-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-poll-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    poll_max = atoi(argv[argc - 1]);
    if (poll_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to poll, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
