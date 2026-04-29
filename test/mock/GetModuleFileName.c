#include <windows.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int get_module_name_max = 0;
static int get_module_name_amount = 0;

DWORD __stdcall tdo_mock_GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    if (get_module_name_amount++ < get_module_name_max) {
        return GetModuleFileNameW(hModule, lpFilename, nSize);
    }
    SetLastError(ERROR_ACCESS_DENIED); // pretend error
    return 0;
}

DWORD __stdcall tdo_mock_GetModuleFileNameA(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    if (get_module_name_amount++ < get_module_name_max) {
        return GetModuleFileNameA(hModule, lpFilename, nSize);
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
    if (is_child) {
        return tdo_runner_main(argc, argv);
    }

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to GetModuleFileName\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-get-module-name-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-get-module-name-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    get_module_name_max = atoi(argv[argc - 1]);
    if (get_module_name_max <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to GetModuleFileName, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
