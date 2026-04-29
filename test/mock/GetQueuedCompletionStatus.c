#include <windows.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int get_queued_max = 0;
static int get_queued_amount = 0;

BOOL __stdcall tdo_mock_GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred, PULONG_PTR lpCompletionKey, LPOVERLAPPED *lpOverlapped, DWORD dwMilliseconds) {
    if (get_queued_amount++ < get_queued_max) {
        return GetQueuedCompletionStatus(CompletionPort, lpNumberOfBytesTransferred, lpCompletionKey, lpOverlapped, dwMilliseconds);
    }
    SetLastError(ERROR_ACCESS_DENIED); // pretend error
    *lpOverlapped = NULL;
    return FALSE;
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
        fprintf(stderr, "Not enough arguments, expected max amount of calls to GetQueuedCompletionStatus\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-get-queued-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-get-queued-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    get_queued_max = atoi(argv[argc - 1]);
    if (get_queued_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to GetQueuedCompletionStatus, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
