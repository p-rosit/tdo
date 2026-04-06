#ifndef TDO_INTERFACE_H
#define TDO_INTERFACE_H
#include <stdbool.h>
#include <stddef.h>
#include "../error.h"

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    typedef int TdoFileDescriptor;
    typedef void* TdoLibrary;

    typedef struct timespec TdoMonotoneTime;
#elif defined(_WIN32)
    #include <windows.h>

    typedef HANDLE TdoFileDescriptor;
    typedef HMODULE TdoLibrary;

    typedef /* ??? */ TdoMonotoneTime;
#else
    #error "Unknown platform"
#endif

struct TdoReadResult {
    size_t bytes_read;
    enum TdoError err;
};

struct TdoReadResult tdo_read_fd(TdoFileDescriptor fd, size_t size, char *buffer);

void tdo_write_fd(TdoFileDescriptor fd, size_t size, char const *data);

TdoMonotoneTime tdo_time_get(void);

struct TdoLibraryLoadResult {
    char const *err;
    TdoLibrary lib;
};

struct TdoArena;

struct TdoLibraryLoadResult tdo_dynamic_library_load(char const *path, struct TdoArena *arena);
void tdo_dynamic_library_unload(TdoLibrary lib);

struct TdoSymbolLoadResult {
    char const *err;
    void *symbol;
};

struct TdoSymbolLoadResult tdo_dynamic_symbol_load(TdoLibrary lib, char const *name, struct TdoArena *arena);

#endif
