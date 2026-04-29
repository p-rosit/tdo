#include <windows.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int create_job_max = 0;
static int create_job_amount = 0;

HANDLE __stdcall tdo_mock_CreateJobObjectW(LPSECURITY_ATTRIBUTES lpJobAttributes, LPCWSTR lpName) {
    if (create_job_amount++ < create_job_max) {
        return CreateJobObjectW(lpJobAttributes, lpName);
    }
    SetLastError(ERROR_ACCESS_DENIED); // pretend error
    return NULL;
}

HANDLE __stdcall tdo_mock_CreateJobObjectA(LPSECURITY_ATTRIBUTES lpJobAttributes, LPCSTR lpName) {
    if (create_job_amount++ < create_job_max) {
        return CreateJobObjectA(lpJobAttributes, lpName);
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
        fprintf(stderr, "Not enough arguments, expected max amount of calls to CreateJobObject\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-create-job-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-create-job-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    create_job_max = atoi(argv[argc - 1]);
    if (create_job_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to CreateJobObject, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
