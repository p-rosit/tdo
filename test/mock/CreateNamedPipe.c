#include <windows.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int create_max = 0;
static int create_amount = 0;

HANDLE __stdcall tdo_mock_CreateNamedPipeW(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances, DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    if (create_amount++ < create_max) {
        return CreateNamedPipeW(lpName, dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize, nInBufferSize, nDefaultTimeOut, lpSecurityAttributes);
    }
    SetLastError(ERROR_ACCESS_DENIED); // pretend error
    return INVALID_HANDLE_VALUE;
}

HANDLE __stdcall tdo_mock_CreateNamedPipeA(LPCSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances, DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    if (create_amount++ < create_max) {
        return CreateNamedPipeA(lpName, dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize, nInBufferSize, nDefaultTimeOut, lpSecurityAttributes);
    }
    SetLastError(ERROR_ACCESS_DENIED); // pretend error
    return INVALID_HANDLE_VALUE;
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
        fprintf(stderr, "Not enough arguments, expected max amount of calls to CreateNamedPipe\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-create-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-create-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    create_max = atoi(argv[argc - 1]);
    if (create_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to CreateNamedPipe, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
