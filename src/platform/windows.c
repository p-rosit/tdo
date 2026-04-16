#include <windows.h>
#include <string.h>
#include "interface.h"

struct TdoReadResult tdo_read_fd(TdoFileDescriptor fd, size_t size, char *buffer) {
//     DWORD bytes_read = 0;
//     bool success = ReadFile(fd, buffer, size, &bytes_read, overlapped);
    
//     enum TdoError err;
//     if (success) {
//         err = TDO_ERROR_OK;
//     } else {
//         DWORD code = GetLastError();

//         if (code == ERROR_IO_PENDING || code == ERROR_IO_INCOMPLETE) {
//             bytes_read = 0;
//             err = TDO_ERROR_WOULD_BLOCK;
//         } else if (code == ERROR_BROKEN_PIPE) {
//             bytes_read = 0;
//             err = TDO_ERROR_OK;
//         } else {
//             LPVOID err_msg;

//             if (FormatMessage(
//                 FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
//                 NULL,
//                 code,
//                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
//                 (LPTSTR) &err_msg,
//                 0,
//                 NULL
//             ) == 0) {
//                 fprintf(stderr, "Read file failed, could not format message for error: %lu\n", code);
//             } else {
//                 fprintf(stderr, "Could not read from file: %s\n", (char*) err_msg);
//                 LocalFree(err_msg);
//             }

//             bytes_read = 0;
//             err = TDO_ERROR_UNKNOWN;
//         }
//     }

//     return (struct TdoReadResult) {
//         .success = success,
//         .bytes_read = bytes_read,
//     };
    return (struct TdoReadResult) {
        .err = TDO_ERROR_OK,
        .bytes_read = 0,
    };
}

void tdo_write_fd(TdoFileDescriptor fd, size_t size, char const *data) {

}

TdoMonotoneTime tdo_time_get(void) {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time;
}

TdoLibrary tdo_dynamic_library_load(char const *path) {
    SetLastError(0);
    return LoadLibrary(path);
}

void tdo_dynamic_library_unload(TdoLibrary lib) {
    FreeLibrary(lib);
}

TdoTestSymbol *tdo_dynamic_symbol_load(TdoLibrary lib, char const *name) {
    SetLastError(0);
    return (TdoTestSymbol*) GetProcAddress(lib, name);
}

char const *tdo_dynamic_get_error(struct TdoArena *arena) {
    DWORD code = GetLastError();
    if (code == 0) return NULL; // no error

    bool could_format;
    LPVOID err_msg;
    if (FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
        NULL,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &err_msg,
        0,
        NULL
    ) == 0) {
        could_format = false;
        err_msg = "Could not format error message";
    } else {
        could_format = true;
    }

    size_t length = strlen((char const*) err_msg);
    char *error = tdo_arena_alloc(arena, sizeof(char), length + 1);
    if (error == NULL) goto error;

    memcpy(error, err_msg, length + 1);

    error:
    if (could_format) LocalFree(err_msg);
    return error;
}

bool tdo_process_status_is_exit(TdoProcessStatus status) {
    // NTSTATUS 'Error' severity starts with 0xC...
    // Check if the top two bits are 11 (binary)
    return (status >> 30) != 3;
}

bool tdo_process_status_is_signal(TdoProcessStatus status) {
    // NTSTATUS 'Error' severity starts with 0xC...
    // Check if the top two bits are 11 (binary)
    return (status >> 30) == 3;
}

bool tdo_process_status_is_stop(TdoProcessStatus status) {
    return false;
}

TdoProcessCode tdo_process_code_exit(TdoProcessStatus status) {
    return status;
}

TdoProcessCode tdo_process_code_signal(TdoProcessStatus status) {
    return status;
}

TdoProcessCode tdo_process_code_stop(TdoProcessStatus status) {
    abort();
}
