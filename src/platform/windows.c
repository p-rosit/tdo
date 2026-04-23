#include <windows.h>
#include <string.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include "../platform.h"

TdoMonotoneTime tdo_time_get(void) {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time;
}

FILE *tdo_file_open_exclusive(char const *path, bool overwrite) {
    HANDLE hFile = CreateFile(
        path,
        GENERIC_WRITE,                          // Open file for writing
        0,                                      // No sharing
        NULL,                                   // Default security
        overwrite ? CREATE_ALWAYS : CREATE_NEW, // overwrite or don't
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        // Map WinAPI error (e.g., ERROR_FILE_EXISTS) to errno (e.g., EEXIST)
        switch (GetLastError()) {
            case ERROR_FILE_EXISTS:
            case ERROR_ALREADY_EXISTS:
                errno = EEXIST;
                break;
            case ERROR_ACCESS_DENIED:
                errno = EACCES;
                break;
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                errno = ENOENT;
                break;
            case ERROR_TOO_MANY_OPEN_FILES:
                errno = EMFILE;
                break;
            case ERROR_DISK_FULL:
                errno = ENOSPC;
                break;
            default:
                errno = EINVAL; // Catch-all for "something else went wrong"
                break;
        }
        return NULL;
    }

    int fd = _open_osfhandle((intptr_t)hFile, _O_WRONLY | _O_BINARY);
    if (fd == -1) {
        // _open_osfhandle sets errno automatically on failure
        CloseHandle(hFile);
        return NULL;
    }

    // 3. Associate the File Descriptor with a FILE stream
    FILE* file = _fdopen(fd, "wb");
    if (file == NULL) {
        _close(fd);
        return NULL;
    }

    return file;
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
    (void)status; // unused
    return false;
}

TdoProcessCode tdo_process_code_exit(TdoProcessStatus status) {
    return status;
}

TdoProcessCode tdo_process_code_signal(TdoProcessStatus status) {
    return status;
}

TdoProcessCode tdo_process_code_stop(TdoProcessStatus status) {
    (void)status; // unused
    abort();
}
