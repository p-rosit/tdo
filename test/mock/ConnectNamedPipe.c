#include <windows.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int connect_max = 0;
static int connect_amount = 0;

BOOL __stdcall tdo_mock_ConnectNamedPipe(HANDLE pipe, LPOVERLAPPED overlapped) {
    if (connect_amount++ < connect_max) {
        return ConnectNamedPipe(pipe, overlapped);
    }
    SetLastError(ERROR_ACCESS_DENIED); // pretend error
    return 0;
}

int tdo_runner_main(int argc, char **argv);
int main(int argc, char **argv) {
    bool is_child = false;
    for (size_t i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--internal-status") == 0) {
            is_child = true;
            break;
        }
    }
    if (is_child) return tdo_runner_main(argc, argv);

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to ConnectNamedPipe\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-connect-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-connect-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    connect_max = atoi(argv[argc - 1]);
    if (connect_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to ConnectNamedPipe, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
