#include "run.c"
#include <stdio.h>
#include <dlfcn.h>

int main(int argc, char **argv) {
    enum TdoError result = TDO_ERROR_UNKNOWN;

    struct TdoArguments args;
    result = tdo_arguments_parse(&args, argc, argv);
    if (result != TDO_ERROR_OK) goto error_parse_args;

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

    FILE *output = stdout;
    if (args.output != NULL) {
        errno = 0;
        output = fopen(args.output, "wb");
        if (errno != 0) {
            perror("Could not open output");
            result = TDO_ERROR_FILE;
            goto error_open_output;
        } else if (output == NULL) {
            fprintf(stderr, "Could not open output file, unknown error\n");
            result = TDO_ERROR_FILE;
            goto error_open_output;
        }
    }

    struct TdoArray test_files = tdo_array_init();
    struct TdoArray tests = tdo_array_init();
    result = tdo_input_parse(arena, string_arena, args.test_file, input, &test_files, &tests);
    if (result != TDO_ERROR_OK) goto error_parse_input;

    dlerror();
    struct TdoFile *files = (struct TdoFile*) test_files.data;
    for (size_t i = 0; i < test_files.length; i++) {
        void *handle = dlopen(files[i].name.bytes, RTLD_NOW);
        char const *error = dlerror();
        if (error != NULL) {
            fprintf(stderr, "%s\n", error);
        } else {
            files[i].dynamic_handle = handle;
        }
    }

    result = tdo_run_all(args, output, arena, tests);

    for (size_t i = 0; i < test_files.length; i++) {
        if (files[i].dynamic_handle == NULL) continue;
        dlclose(files[i].dynamic_handle);
    }
    error_parse_input:
    if (output != stdout) fclose(output);
    error_open_output:
    fclose(input);
    error_open_input:
    tdo_arena_deinit(string_arena);
    error_init_string_arena:
    tdo_arena_deinit(arena);
    error_parse_args:
    return result;
}
