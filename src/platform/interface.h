#ifndef TDO_INTERFACE_H
#define TDO_INTERFACE_H
#include <stdbool.h>
#include <stddef.h>
#include "../error.h"

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    typedef int TdoFileDescriptor;

    typedef struct timespec TdoMonotoneTime;
#elif defined(_WIN32)
    #include <windows.h>
    typedef HANDLE TdoFileDescriptor;

    typedef /* ??? */ TdoMonotoneTime;
#else
    #error "Unknown platform"
#endif

struct TdoReadResult {
    size_t bytes_read;
    enum TdoError err;
};

struct TdoReadResult tdo_read_fd(TdoFileDescriptor fd, size_t size, char *buffer);

TdoMonotoneTime tdo_time_get(void);

#endif
