#include <unistd.h>
#include <errno.h>

#ifndef TDO_READ_AMOUNT
    #error "An amount of successful calls to read must be defined"
#endif

static int read_amount = 0;

ssize_t tdo_mock_read(int fd, char *buffer, size_t size) {
    if (read_amount++ < TDO_READ_AMOUNT) {
        return read(fd, buffer, size);
    }
    errno = EBADF;
    return -1;
}
