#include <windows.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int create_port_max = 0;
static int create_port_amount = 0;

HANDLE __stdcall tdo_mock_CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort, ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads) {
    if (create_port_amount++ < create_port_max) {
        return CreateIoCompletionPort(FileHandle, ExistingCompletionPort, CompletionKey, NumberOfConcurrentThreads);
    }
    SetLastError(ERROR_ACCESS_DENIED); // pretend error
    return NULL;
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
    if (is_child) {
        return tdo_runner_main(argc, argv);
    }

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to CreateIoCompletionPort\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-create-port-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-create-port-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    create_port_max = atoi(argv[argc - 1]);
    if (create_port_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to CreateIoCompletionPort, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
