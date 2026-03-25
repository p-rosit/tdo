#include "string.c"
#include "arguments.c"
#include <stdio.h>

int main(int argc, char **argv) {
    enum TdoError result = TDO_ERROR_UNKNOWN;

    struct TdoArguments args;
    result = tdo_arguments_parse(&args, argc, argv);
    if (result != TDO_ERROR_OK) goto error_parse;

    struct TdoArena *arena = tdo_arena_init(1024);
    if (arena == NULL) return -1;
    struct TdoArena *string_arena = tdo_arena_init(4);
    if (string_arena == NULL) {
        result = TDO_ERROR_MEMORY;
        goto error_init_string_arena;
    }

    FILE *input = fopen(args.test_file, "rb");
    if (input == NULL) {
        result = TDO_ERROR_FILE;
        goto error_open_input;
    }

    fclose(input);
    error_open_input:
    tdo_arena_deinit(string_arena);
    error_init_string_arena:
    tdo_arena_deinit(arena);
    error_parse:
    return result;
}
