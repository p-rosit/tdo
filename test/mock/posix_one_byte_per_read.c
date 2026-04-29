#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

ssize_t tdo_mock_read(int fd, char *buffer, size_t size) {
    if (size < 1) {
        fprintf(stderr, "Attempting to read zero bytes???\n");
        abort();
    }
    return read(fd, buffer, 1);
}
