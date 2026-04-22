#include "../platform.h"
#include <errno.h>
#include <stdio.h>

#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

TdoMonotoneTime tdo_time_get(void) {
    struct timespec time = {0};
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time;
}

FILE *tdo_file_open_exclusive(char const *path, bool overwrite) {
    int open_flags = O_WRONLY | O_CREAT;
    if (!overwrite) open_flags |= O_EXCL;

    errno = 0;
    int output_fd = open(path, open_flags, S_IRUSR | S_IWUSR);
    if (errno != 0) {
        return NULL;
    } else if (output_fd == -1) {
        errno = EBADF; // ???
        return NULL;
    }

    errno = 0;
    FILE *output = fdopen(output_fd, "wb");
    if (errno != 0) {
        close(output_fd);
        return NULL;
    } else if (output == NULL) {
        close(output_fd);
        errno = EBADF; // ???
        return NULL;
    }

    return output;
}

TdoLibrary tdo_dynamic_library_load(char const *path) {
    dlerror();
    return dlopen(path, RTLD_NOW);
}

void tdo_dynamic_library_unload(TdoLibrary lib) {
    dlclose(lib);
}

TdoTestSymbol *tdo_dynamic_symbol_load(TdoLibrary lib, char const *name) {
    dlerror();
    return (TdoTestSymbol*) dlsym(lib, name);
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
