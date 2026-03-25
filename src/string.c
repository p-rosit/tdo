#include "arena.c"
#include <stdbool.h>
#include <string.h>

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

    bool success = tdo_arena_resize(arena, string->bytes, sizeof(char), allocation_length);
    if (!success) {
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
