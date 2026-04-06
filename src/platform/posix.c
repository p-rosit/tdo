#define TDO_FILE_DESCRIPTOR int
#include "interface.h"
#include <errno.h>
#include <stdio.h>

#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

struct TdoReadResult tdo_read_fd(TdoFileDescriptor fd, size_t size, char *buffer) {
    errno = 0;
    ssize_t br = read(fd, buffer, sizeof(buffer));

    size_t bytes_read;
    enum TdoError err;

    if (br >= 0) {
        bytes_read = br;
        err = TDO_ERROR_OK;
    } else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        bytes_read = 0;
        err = TDO_ERROR_WOULD_BLOCK;
    } else {
        perror("Could not read from file");
        bytes_read = 0;
        err = TDO_ERROR_UNKNOWN;
    }

    return (struct TdoReadResult) {
        .bytes_read = bytes_read,
        .err = err,
    };
}

void tdo_write_fd(TdoFileDescriptor fd, size_t size, char const *data) {
    ssize_t bytes_written = write(fd, data, size);
    (void)bytes_written;
}

TdoMonotoneTime tdo_time_get(void) {
    struct timespec time = {0};
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time;
}

struct TdoLibraryLoadResult tdo_dynamic_library_load(char const *path, struct TdoArena *arena) {
    dlerror();
    void *handle = dlopen(path, RTLD_NOW);
    char const *error = dlerror();
    if (error != NULL) {
        handle = NULL;
    }

    return (struct TdoLibraryLoadResult) {
        .err = error,
        .lib = handle,
    };
}

void tdo_dynamic_library_unload(TdoLibrary lib) {
    dlclose(lib);
}

struct TdoSymbolLoadResult tdo_dynamic_symbol_load(TdoLibrary lib, char const *name, struct TdoArena *arena) {
    dlerror();
    void *symbol = dlsym(lib, name);
    char const *error = dlerror();
    if (error != NULL) {
        symbol = NULL;
    }

    return (struct TdoSymbolLoadResult) {
        .err = error,
        .symbol = symbol,
    };
}
