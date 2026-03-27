#include "arena.c"
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>

struct TdoString {
    char *bytes;
    size_t length;
};

struct TdoString tdo_string_init(void) {
    return (struct TdoString) { .bytes = NULL, .length = 0 };
}

bool tdo_string_append(struct TdoString *string, struct TdoArena *arena, size_t length, char const *data) {
    if (string->bytes == NULL && string->length > 0) {
        return false;
    } else if (string->bytes == NULL && string->length == 0) {
        char *bytes = tdo_arena_alloc(arena, sizeof(char), 1);
        if (bytes == NULL) return false;

        *bytes = '\0';
        string->bytes = bytes;
        string->length = 0;
    }

    size_t allocation_length = string->length;

    if (allocation_length > SIZE_MAX - 1) {
        fprintf(stderr, "string length overflow: %zu + 1\n", string->length);
        abort();
    }
    allocation_length += 1;

    if (allocation_length > SIZE_MAX - length) {
        fprintf(stderr, "string length overflow: %zu + 1 + %zu\n", string->length, length);
        abort();
    }
    allocation_length += length;

    if (!tdo_arena_resize(arena, string->bytes, sizeof(char), allocation_length)) {
        char *bytes = tdo_arena_alloc(arena, sizeof(char), allocation_length);
        if (bytes == NULL) return false;

        memcpy(bytes, string->bytes, string->length);
        string->bytes = bytes;
    }

    memcpy(string->bytes + string->length, data, length);
    string->length = string->length + length;
    string->bytes[string->length] = '\0';

    return true;
}

struct TdoLog {
    int fd;
    struct TdoString data;
    size_t capacity;
};

struct TdoLog tdo_log_init(int fd) {
    return (struct TdoLog) {
        .fd = fd,
        .data = (struct TdoString) {
            .bytes = NULL,
            .length = 0,
        },
        .capacity = 0,
    };
}

void tdo_log_reset(struct TdoLog *log) {
    log->data.length = 0;
}

bool tdo_log_drain(struct TdoLog *log, struct TdoArena *arena) {
    bool result = true;

    char buffer[1024];
    while (true) {
        errno = 0;
        ssize_t bytes_read = read(log->fd, buffer, sizeof(buffer));

        if (bytes_read > 0) {
            result = tdo_string_append(&log->data, arena, (size_t) bytes_read, buffer);
            if (!result) return false;
        } else if (bytes_read == 0) {
            break;
        } else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        } else {
            perror("Could not read from pipe");
            return false;
        }
    }

    return result;
}
