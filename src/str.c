#include "platform.h"
#include "error.h"
#include "arena.h"
#include "str.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

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

bool tdo_string_clone(struct TdoString *copy, struct TdoArena *arena, struct TdoString string) {
    *copy = tdo_string_init();
    return tdo_string_append(copy, arena, string.length, string.bytes);
}

struct TdoLog tdo_log_init(TdoFileDescriptor fd) {
    return (struct TdoLog) {
        .fd = fd,
        .data = (struct TdoString) {
            .bytes = NULL,
            .length = 0,
        },
        .capacity = 0,
    };
}

void tdo_log_reset(struct TdoLog *log, TdoFileDescriptor fd) {
    log->fd = fd;
    log->data.length = 0;
}
