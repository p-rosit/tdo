#ifndef TDO_TEST_H
#define TDO_TEST_H
#include "platform.h"
#include "str.h"
#include <stdlib.h>

struct TdoFile {
    TdoLibrary library;
    struct TdoString name;
};

struct TdoSymbol {
    struct TdoFile *file;
    struct TdoString name;
};

enum TdoFixtureKind {
    TDO_FIXTURE_BEFORE,
    TDO_FIXTURE_AFTER,
};

struct TdoFixture {
    struct TdoSymbol symbol;
    enum TdoFixtureKind kind;
};

struct TdoArray {
    void *data;
    size_t length;
    size_t capacity;
};

struct TdoTest {
    struct TdoSymbol symbol;
    struct TdoArray fixtures;
};

struct TdoArray tdo_array_init(void);
enum TdoError tdo_array_ensure_space(struct TdoArray *array, struct TdoArena *arena, size_t type_size, size_t additional_capacity);
enum TdoError tdo_array_append(struct TdoArray *array, struct TdoArena *arena, size_t type_size, void *item);

enum TdoError tdo_read_line(struct TdoString *string, struct TdoArena *arena, FILE *file);
enum TdoError tdo_test_parse_file(struct TdoString *file, struct TdoString *line, char const *file_name, size_t line_number);
enum TdoError tdo_test_parse_name(struct TdoString *name, struct TdoString *line, char const *file_name, size_t line_number);
enum TdoError tdo_test_parse_symbol(struct TdoString *line, char const *input_file_name, size_t line_number, struct TdoArena *arena, struct TdoArena *string_arena, struct TdoSymbol *symbol, struct TdoArray *test_files);
enum TdoError tdo_test_parse_test(struct TdoString *line, char const *file_name, size_t line_number, struct TdoArena *arena, struct TdoArena *string_arena, struct TdoTest *test, struct TdoArray *test_files);
enum TdoError tdo_test_parse_fixture(struct TdoString *line, char const *file_name, size_t line_number, struct TdoArena *arena, struct TdoArena *string_arena, struct TdoTest *test, struct TdoArray *test_files);
enum TdoError tdo_input_parse(struct TdoArena *arena, struct TdoArena *string_arena, char const *file_name, FILE *input_file, char const *single_line, struct TdoArray *test_files, struct TdoArray *tests);

#endif
