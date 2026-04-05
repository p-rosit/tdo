#define _XOPEN_SOURCE 600
#include "run.c"
#include <stdio.h>
#include <dlfcn.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    enum TdoError result = TDO_ERROR_UNKNOWN;

    struct TdoArguments args;
    result = tdo_arguments_parse(&args, argc, argv);
    if (result != TDO_ERROR_OK) goto error_parse_args;

    struct TdoArena *arena = tdo_arena_init(1024);
    if (arena == NULL) {
        fprintf(stderr, "Could not initialize arena\n");
        result = TDO_ERROR_MEMORY;
        goto error_init_arena;
    }
    struct TdoArena *string_arena = tdo_arena_init(1024);
    if (string_arena == NULL) {
        fprintf(stderr, "Could not initialize string arena\n");
        result = TDO_ERROR_MEMORY;
        goto error_init_string_arena;
    }

    FILE *input = stdin;
    if (args.test_file != NULL) {
        errno = 0;
        input = fopen(args.test_file, "rb");
        if (errno != 0) {
            perror("Could not open input");
            result = TDO_ERROR_FILE;
            goto error_open_input;
        } else if (input == NULL) {
            fprintf(stderr, "Could not open input file, unknown error\n");
            result = TDO_ERROR_FILE;
            goto error_open_input;
        }
    } else {
        fprintf(stderr, "Reading tests from stdin:\n");
    }

    FILE *output = stdout;
    if (args.output != NULL) {
        errno = 0;
        int output_fd = open(args.output, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (errno != 0) {
            perror("Could not open output");
            result = TDO_ERROR_FILE;
            goto error_open_output;
        } else if (output_fd == -1) {
            fprintf(stderr, "Could not open output file, unknown error\n");
            result = TDO_ERROR_FILE;
            goto error_open_output;
        }

        errno = 0;
        output = fdopen(output_fd, "wb");
        if (errno != 0) {
            close(output_fd);
            perror("Could not create output file from file descriptor");
            result = TDO_ERROR_FILE;
            goto error_open_output;
        } else if (output == NULL) {
            close(output_fd);
            fprintf(stderr, "Could not create output file from file descriptor, unknown error\n");
            result = TDO_ERROR_FILE;
            goto error_open_output;
        }
    }

    struct TdoArray test_files = tdo_array_init();
    struct TdoArray tests = tdo_array_init();
    result = tdo_input_parse(arena, string_arena, args.test_file != NULL ? args.test_file : "<stdin>", input, &test_files, &tests);
    if (result != TDO_ERROR_OK) goto error_parse_input;

    struct TdoArenaState state = tdo_arena_state_get(arena);
    struct TdoFile *files = (struct TdoFile*) test_files.data;
    for (size_t i = 0; i < test_files.length; i++) {
        tdo_arena_state_set(arena, state);

        struct TdoLibraryLoadResult result = tdo_dynamic_library_load(files[i].name.bytes, arena);
        if (result.err != NULL) {
            fprintf(stderr, "%s\n", result.err);
            files[i].library = NULL;
        } else {
            files[i].library = result.lib;
        }
    }
    tdo_arena_state_set(arena, state);

    result = tdo_run_all(args, output, arena, tests);

    for (size_t i = 0; i < test_files.length; i++) {
        if (files[i].library == NULL) continue;
        tdo_dynamic_library_unload(files[i].library);
    }
    error_parse_input:
    if (output != stdout) fclose(output);
    error_open_output:
    if (input != stdin) fclose(input);
    error_open_input:
    tdo_arena_deinit(string_arena);
    error_init_string_arena:
    tdo_arena_deinit(arena);
    error_init_arena:
    error_parse_args:
    return result;
}
