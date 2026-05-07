#include "error.h"
#include "str.h"
#include "test.h"
#include <string.h>
#include <ctype.h>
#include <stdint.h>

struct TdoArray tdo_array_init(void) {
    return (struct TdoArray) { .data = NULL, .length = 0, .capacity = 0 };
}

enum TdoError tdo_array_ensure_space(struct TdoArray *array, struct TdoArena *arena, size_t type_size, size_t additional_capacity) {
    if (array->length > SIZE_MAX - additional_capacity) return TDO_ERROR_MEMORY; // array too large
    size_t requested_capacity = array->length + additional_capacity;

    if (requested_capacity <= array->capacity) return TDO_ERROR_OK;

    size_t new_capacity = array->capacity;
    while (new_capacity < requested_capacity) {
        if (array->capacity > SIZE_MAX / 2) return TDO_ERROR_MEMORY;
        new_capacity *= 2;
        if (new_capacity <= 0) new_capacity = 4;
    }

    if (!tdo_arena_resize(arena, array->data, type_size, new_capacity)) {
        char *new_array = tdo_arena_alloc(arena, type_size, new_capacity);
        if (new_array == NULL) return TDO_ERROR_MEMORY;

        if (array->length > SIZE_MAX / type_size) return TDO_ERROR_MEMORY;
        size_t byte_length = array->length * type_size;

        if (array->data != NULL) memcpy(new_array, array->data, byte_length);
        array->data = new_array;
    }

    array->capacity = new_capacity;

    return TDO_ERROR_OK;
}

enum TdoError tdo_array_append(struct TdoArray *array, struct TdoArena *arena, size_t type_size, void *item) {
    enum TdoError result = tdo_array_ensure_space(array, arena, type_size, 1);
    if (result != TDO_ERROR_OK) return result;

    if (array->length > SIZE_MAX / type_size) return TDO_ERROR_MEMORY;
    size_t byte_length = array->length * type_size;
    memcpy((char*) array->data + byte_length, item, type_size);
    array->length += 1;
    return TDO_ERROR_OK;
}

enum TdoError tdo_read_line(struct TdoString *string, struct TdoArena *arena, FILE *file) {
    *string = tdo_string_init();

    int c;
    while ((c = fgetc(file)) != EOF) {
        if (c == '\n') break;

        char b = (char) c;
        if (!tdo_string_append(string, arena, 1, &b)) return TDO_ERROR_MEMORY;
    }

    if (c == EOF && string->length == 0) return TDO_ERROR_FILE;
    return TDO_ERROR_OK;
}

enum TdoError tdo_test_parse_file(struct TdoString *file, struct TdoString *line, char const *file_name, size_t line_number) {
    *file = (struct TdoString) { .bytes = line->bytes, .length = 0 };

    bool delimiter_found = false;
    while (line->length > 0) {
        if (line->bytes[0] == ':') {
            if (line->length > 1 && line->bytes[1] == ':') {
                line->bytes += 2;
                line->length -= 2;
                delimiter_found = true;
                break;
            }
        }

        int c = line->bytes[0];
        if (isspace(c)) break;

        line->bytes += 1;
        line->length -= 1;
        file->length += 1;
    }

    if (!delimiter_found) {
        fprintf(stderr, "%s:%zu Did not find delimiter '::' in definition, found file '%.*s'\n", file_name, line_number, (int) file->length, file->bytes);
        return TDO_ERROR_EOF;
    }

    return TDO_ERROR_OK;
}

enum TdoError tdo_test_parse_name(struct TdoString *name, struct TdoString *line, char const *file_name, size_t line_number) {
    *name = (struct TdoString) { .bytes = line->bytes, .length = 0 };

    while (line->length > 0) {
        if (line->bytes[0] == ':') {
            if (line->length > 1 && line->bytes[1] == ':') {
                name->length += 2;
                fprintf(stderr, "%s:%zu: Found unexpected delimiter '::' in name '%.*s'\n", file_name, line_number, (int) name->length, name->bytes);
                return TDO_ERROR_EOF; // unexpected delimiter
            }
        }

        int c = line->bytes[0];
        if (isspace(c)) break;

        line->bytes += 1;
        line->length -= 1;
        name->length += 1;
    }

    return TDO_ERROR_OK;
}

enum TdoError tdo_test_parse_symbol(struct TdoString *line, char const *input_file_name, size_t line_number, struct TdoArena *arena, struct TdoArena *string_arena, struct TdoSymbol *symbol, struct TdoArray *test_files) {
    struct TdoString file_name;
    enum TdoError result = tdo_test_parse_file(&file_name, line, input_file_name, line_number);
    if (result != TDO_ERROR_OK) return result;

    if (file_name.length <= 0) {
        fprintf(stderr, "%s:%zu: Empty library name\n", input_file_name, line_number);
        return TDO_ERROR_EOF; // library name empty
    }

    struct TdoString name;
    result = tdo_test_parse_name(&name, line, input_file_name, line_number);
    if (result != TDO_ERROR_OK) return result;

    if (name.length <= 0) {
        fprintf(stderr, "%s:%zu: Empty symbol name\n", input_file_name, line_number);
        return TDO_ERROR_EOF; // symbol name empty
    }

    struct TdoFile *file = NULL;
    struct TdoFile *files = test_files->data;
    for (size_t i = 0; i < test_files->length; i++) {
        struct TdoFile *f = &files[i];
        if (file_name.length == f->name.length && strncmp(file_name.bytes, f->name.bytes, file_name.length < f->name.length ? file_name.length : f->name.length) == 0) {
            file = &files[i];
            break;
        }
    }

    if (file == NULL) {
        struct TdoFile f = (struct TdoFile) {
            .library = NULL,
            .name = tdo_string_init(),
        };
        if (!tdo_string_clone(&f.name, string_arena, file_name)) return TDO_ERROR_MEMORY;

        result = tdo_array_append(test_files, arena, sizeof(struct TdoFile), &f);
        if (result != TDO_ERROR_OK) return result;

        file = &((struct TdoFile*) test_files->data)[test_files->length - 1];
    }

    *symbol = (struct TdoSymbol) { .file = file, .name = tdo_string_init() };
    if (!tdo_string_clone(&symbol->name, string_arena, name)) return TDO_ERROR_MEMORY;

    return TDO_ERROR_OK;
}

enum TdoError tdo_test_parse_test(struct TdoString *line, char const *file_name, size_t line_number, struct TdoArena *arena, struct TdoArena *string_arena, struct TdoTest *test, struct TdoArray *test_files) {
    int c;
    while (line->length > 0 && isspace(c = line->bytes[0])) {
        line->length -= 1;
        line->bytes += 1;
    }
    if (line->length == 0) return TDO_ERROR_EOF;

    if (line->bytes[0] == '#') {
        // comment
        return TDO_ERROR_EOF;
    } else if (strncmp(line->bytes, "test::", 6) != 0) {
        size_t prefix = line->length > 6 ? 6 : line->length;
        fprintf(stderr, "%s:%zu Expected test symbol to start with 'test::', got '%.*s'\n", file_name, line_number, (int) prefix, line->bytes);
        return TDO_ERROR_PREFIX;
    }
    line->bytes += 6;
    line->length -= 6;
    
    struct TdoTest t = (struct TdoTest) {
        .symbol = {
            .file = NULL,
            .name = tdo_string_init(),
        },
        .fixtures = tdo_array_init(),
    };
    enum TdoError result = tdo_test_parse_symbol(line, file_name, line_number, arena, string_arena, &t.symbol, test_files);
    if (result != TDO_ERROR_OK) return result;

    *test = t;
    return TDO_ERROR_OK;
}

enum TdoError tdo_test_parse_fixture(struct TdoString *line, char const *file_name, size_t line_number, struct TdoArena *arena, struct TdoArena *string_arena, struct TdoTest *test, struct TdoArray *test_files) {
    int c;
    while (isspace(c = line->bytes[0])) {
        line->length -= 1;
        line->bytes += 1;
    }
    if (line->length <= 0) return TDO_ERROR_OK;

    enum TdoFixtureKind kind;
    if (strncmp(line->bytes, "before::", 8) == 0) {
        kind = TDO_FIXTURE_BEFORE;
        line->bytes += 8;
        line->length -= 8;
    } else if (strncmp(line->bytes, "after::", 7) == 0) {
        kind = TDO_FIXTURE_AFTER;
        line->bytes += 7;
        line->length -= 7;
    } else {
        fprintf(stderr, "%s:%zu Expected fixture symbol to start with 'before::' or 'after::', got '%.*s'\n", file_name, line_number, line->length < 8 ? (int) line->length : 8, line->bytes);
        return TDO_ERROR_PREFIX;
    }

    struct TdoSymbol symbol;
    enum TdoError result = tdo_test_parse_symbol(line, file_name, line_number, arena, string_arena, &symbol, test_files);
    if (result != TDO_ERROR_OK) return result;

    struct TdoFixture fixture = { .kind = kind, .symbol = symbol };
    return tdo_array_append(&test->fixtures, arena, sizeof(struct TdoFixture), &fixture);
}

enum TdoError tdo_input_parse(struct TdoArena *arena, struct TdoArena *string_arena, char const *file_name, FILE *input_file, char const *single_line, struct TdoArray *test_files, struct TdoArray *tests) {
    enum TdoError result = TDO_ERROR_UNKNOWN;
    struct {
        struct TdoArray test_files;
        struct TdoArray tests;
        struct TdoArenaState state;
        struct TdoArenaState string_state;
    } old_state = {
        .test_files = *test_files,
        .tests = *tests,
        .state = tdo_arena_state_get(arena),
        .string_state = tdo_arena_state_get(string_arena),
    };

    struct TdoArena *temp_arena = tdo_arena_init(single_line == NULL ? 1024 : 1);
    if (temp_arena == NULL) return TDO_ERROR_MEMORY;

    size_t line_number = 0;
    while (true) {
        line_number += 1;
        tdo_arena_state_clear(temp_arena);

        struct TdoString line;
        if (single_line == NULL) {
            result = tdo_read_line(&line, temp_arena, input_file);
            if (result == TDO_ERROR_FILE) break;
            else if (result != TDO_ERROR_OK) goto error;
        } else {
            if (line_number > 1) break;
            line = (struct TdoString) { .bytes=(char*) single_line, .length=strlen(single_line) };
        }

        struct TdoTest test;
        result = tdo_test_parse_test(&line, file_name, line_number, arena, string_arena, &test, test_files);
        if (result == TDO_ERROR_EOF || result == TDO_ERROR_PREFIX) {
            // not resetting the arena leaks memory but we might've seen a new
            // dynamic library for the first time so it's not safe to reset
            goto next_loop;
        } else if (result != TDO_ERROR_OK) {
            goto error;
        }

        while (line.length > 0) {
            result = tdo_test_parse_fixture(&line, file_name, line_number, arena, string_arena, &test, test_files);
            if (result == TDO_ERROR_EOF) {
                break;
            } else if (result == TDO_ERROR_PREFIX) {
                // not resetting the arena leaks memory but we might've seen a new
                // dynamic library for the first time so it's not safe to reset
                goto next_loop;
            } else if (result != TDO_ERROR_OK) {
                goto error;
            }
        }

        result = tdo_array_append(tests, arena, sizeof(struct TdoTest), &test);
        if (result != TDO_ERROR_OK) return result;

        next_loop:
        (void)NULL; // label wants an expression...
    }

    result = TDO_ERROR_OK;
    error:

    tdo_arena_deinit(temp_arena);
    if (result != TDO_ERROR_OK) {
        tdo_arena_state_set(arena, old_state.state);
        tdo_arena_state_set(string_arena, old_state.string_state);
        *test_files = old_state.test_files;
        *tests = old_state.tests;
    }
    return result;
}
