#ifndef TDO_INTERFACE_H
#define TDO_INTERFACE_H
#include <stdbool.h>
#include <stddef.h>
#include "../error.h"
#include "../arena.h"

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    #define TDO_POSIX
    typedef int TdoFileDescriptor;
    #define TDO_FILE_DESCRIPTOR_INVALID (-1)

    typedef void* TdoLibrary;

    typedef struct timespec TdoMonotoneTime;

    typedef int TdoProcessStatus;
    typedef int TdoProcessCode;
    #define TDO_PROCESS_CODE_FORMAT "%d"
#elif defined(_WIN32)
    #define TDO_WINDOWS
    #include <windows.h>

    typedef HANDLE TdoFileDescriptor;
    #define TDO_FILE_DESCRIPTOR_INVALID (INVALID_HANDLE_VALUE)

    typedef HMODULE TdoLibrary;

    typedef LARGE_INTEGER TdoMonotoneTime;

    typedef DWORD TdoProcessStatus;
    typedef DWORD TdoProcessCode;
    #define TDO_PROCESS_CODE_FORMAT "%lu"
#else
    #error "Unknown platform"
#endif

TdoMonotoneTime tdo_time_get(void);

typedef void TdoTestSymbol(void);

TdoLibrary tdo_dynamic_library_load(char const *path);
void tdo_dynamic_library_unload(TdoLibrary lib);
TdoTestSymbol *tdo_dynamic_symbol_load(TdoLibrary lib, char const *name);
char const *tdo_dynamic_get_error(struct TdoArena *arena);

bool tdo_process_status_is_exit(TdoProcessStatus status);
bool tdo_process_status_is_signal(TdoProcessStatus status);
bool tdo_process_status_is_stop(TdoProcessStatus status);

TdoProcessCode tdo_process_code_exit(TdoProcessStatus status);
TdoProcessCode tdo_process_code_signal(TdoProcessStatus status);
TdoProcessCode tdo_process_code_stop(TdoProcessStatus status);

#endif
