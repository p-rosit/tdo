#include "string.c"
#include "arguments.c"

int main(int argc, char **argv) {
    enum TdoError result = TDO_ERROR_UNKNOWN;

    struct TdoArguments args;
    result = tdo_arguments_parse(&args, argc, argv);
    if (result != TDO_ERROR_OK) goto error_parse;

    struct TdoArena *arena = tdo_arena_init(1024);
    if (arena == NULL) return -1;
    struct TdoArena *string_arena = tdo_arena_init(4);
    if (string_arena == NULL) {
        tdo_arena_deinit(arena);
        return -1;
    }

    tdo_arena_deinit(string_arena);
    tdo_arena_deinit(arena);
    error_parse:
    return result;
}
