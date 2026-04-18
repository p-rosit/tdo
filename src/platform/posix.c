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

TdoMonotoneTime tdo_time_get(void) {
    struct timespec time = {0};
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time;
}

struct TdoLibraryLoadResult tdo_dynamic_library_load(char const *path) {
    dlerror();
    void *handle = dlopen(path, RTLD_NOW);
    return lib;
}

void tdo_dynamic_library_unload(TdoLibrary lib) {
    dlclose(lib);
}

TdoLibrary tdo_dynamic_symbol_load(TdoLibrary lib, char const *name) {
    dlerror();
    void *symbol = dlsym(lib, name);
    return symbol;
}

char const *tdo_dynamic_get_error(struct TdoArena *arena) {
    return dlerror();
}

bool tdo_process_status_is_exit(TdoProcessStatus status) {
    return WIFEXITED(status);
}

bool tdo_process_status_is_signal(TdoProcessStatus status) {
    return WIFSIGNALED(status);
}

bool tdo_process_status_is_stop(TdoProcessStatus status) {
    return WIFSTOPPED(status);
}

TdoProcessCode tdo_process_code_exit(TdoProcessStatus status) {
    return WEXITSTATUS(status);
}

TdoProcessCode tdo_process_code_signal(TdoProcessStatus status) {
    return WTERMSIG(status);
}

TdoProcessCode tdo_process_code_stop(TdoProcessStatus status) {
    return WSTOPSIG(status);
}
