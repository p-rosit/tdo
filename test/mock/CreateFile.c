#include <windows.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int create_file_max = 0;
static int create_file_amount = 0;

HANDLE __stdcall tdo_mock_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (create_file_amount++ < create_file_max) {
        return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    SetLastError(ERROR_ACCESS_DENIED); // pretend error
    return INVALID_HANDLE_VALUE;
}

HANDLE __stdcall tdo_mock_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (create_file_amount++ < create_file_max) {
        return CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
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
    if (is_child) {
        create_file_max = 1;
        return tdo_runner_main(argc, argv);
    }

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to CreateFile\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-create-file-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-create-file-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    create_file_max = atoi(argv[argc - 1]);
    if (create_file_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to CreateFile, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
