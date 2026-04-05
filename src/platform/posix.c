#define TDO_FILE_DESCRIPTOR int
#include "interface.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

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
