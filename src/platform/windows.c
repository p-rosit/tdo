#include <windows.h>
#include "interface.h"

struct TdoReadResult tdo_read_fd(TdoFileDescriptor fd, size_t size, char *buffer);

void tdo_write_fd(TdoFileDescriptor fd, size_t size, char const *data);

TdoMonotoneTime tdo_time_get(void) {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time;
}

struct TdoLibraryLoadResult tdo_dynamic_library_load(char const *path, struct TdoArena *arena);

void tdo_dynamic_library_unload(TdoLibrary lib);

struct TdoSymbolLoadResult tdo_dynamic_symbol_load(TdoLibrary lib, char const *name, struct TdoArena *arena);

bool tdo_process_status_is_exit(TdoProcessStatus status);

bool tdo_process_status_is_signal(TdoProcessStatus status);

bool tdo_process_status_is_stop(TdoProcessStatus status);

TdoProcessCode tdo_process_code_exit(TdoProcessStatus status);

TdoProcessCode tdo_process_code_signal(TdoProcessStatus status);

TdoProcessCode tdo_process_code_stop(TdoProcessStatus status);
