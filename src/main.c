#define _XOPEN_SOURCE 600

#ifndef TDO_BUILD_TEST
    #include "arena.c"
    #include "str.c"
    #include "arguments.c"
    #include "platform.c"
    #include "test.c"
    #include "run.c"
#else
    #include "arena.h"
    #include "str.h"
    #include "arguments.h"
    #include "platform.h"
    #include "test.h"
    #include "run.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

static const char *tdo_help_text =
    "Usage: tdo [OPTIONS] [TEST_FILE]\n"
    "       tdo [OPTIONS] -t \"TEST_DEFINITION\"\n"
    "       cat tests.txt | tdo [OPTIONS]\n"
    "\n"
    "A C99 test runner that executes tests defined by shared library symbols.\n"
    "Every test is executed in a separate process which ensures that they do not\n"
    "interact.\n"
    "\n"
    "Arguments:\n"
    "  TEST_FILE             Path to a file containing test definitions. If omitted \n"
    "                        and no -t flag is provided, definitions are read \n"
    "                        from standard input (stdin).\n"
    "\n"
    "Options:\n"
    "  -t \"TEST_DEFINITION\"  Run a single test definition string directly.\n"
    "  -j [N]                Run tests in parallel using N processes (default: 1).\n"
    "  --format FMT          Select output format: 'human' or 'json' (default: human).\n"
    "  -o FILE               Write results to the specified FILE.\n"
    "  -f                    Force overwrite the output file if it already exists.\n"
    "  --timeout SECONDS     Set a maximum execution time per test (default: 5.0).\n"
    "  -h, --help            Display this help message and exit.\n"
    "\n"
    "Test Definition Format:\n"
    "  Definitions follow a \"test-first\" structure:\n"
    "    test::PATH::SYMBOL [before::PATH::SYMBOL] [after::PATH::SYMBOL] ...\n"
    "\n"
    "  - PATH:   The path to the shared library (e.g., .so, .dll, or .dylib).\n"
    "  - SYMBOL: The name of the exported function to execute.\n"
    "\n"
    "  Definitions can include any number of 'before' and 'after' hooks.\n"
    "  Example:\n"
    "    test::./lib.so::test_math before::./lib.so::setup after::./lib.so::clean\n"
;

int main(int argc, char **argv) {
    enum TdoError result = TDO_ERROR_UNKNOWN;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            fprintf(stderr, "%s", tdo_help_text);
            return TDO_ERROR_OK;
        }
    }

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
    
    FILE *status = NULL;
    if (args.internal_status != NULL) {
        errno = 0;
        status = fopen(args.internal_status, "wb");
        if (errno != 0) {
            perror("Could not open status pipe");
            goto error_open_status;
        }
    }
    if (args.internal_status != NULL && status == NULL) {
        fprintf(stderr, "Unknown error, could not open status pipe\n");
        goto error_open_status;
    }

    char const *file_name;
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
        file_name = args.test_file;

        if (args.internal_status == NULL) fprintf(stderr, "Reading tests from input file '%s'\n", args.test_file);
    } else if (args.single_test) {
        if (args.internal_status == NULL) fprintf(stderr, "Reading test from command line\n");
        file_name = "<cmd>";
    } else {
        if (args.internal_status == NULL) fprintf(stderr, "Reading tests from stdin:\n");
        file_name = "<stdin>";
    }

    FILE *output = stdout;
    if (args.output != NULL) {
        errno = 0;
        output = tdo_file_open_exclusive(args.output, args.overwrite);
        if (errno == EEXIST) {
            fprintf(stderr, "Output file '%s' already exists, overwrite with '-f'\n", args.output);
            result = TDO_ERROR_FILE;
            goto error_open_output;
        } else if (output == NULL) {
            perror("Could not open output file for writing");
            result = TDO_ERROR_UNKNOWN;
        }
    }

    struct TdoArray test_files = tdo_array_init();
    struct TdoArray tests = tdo_array_init();
    {
        result = tdo_input_parse(arena, string_arena, file_name, input, args.single_test, &test_files, &tests);
        if (result != TDO_ERROR_OK) goto error_parse_input;

        if (args.single_test != NULL) {
            if (tests.length < 1) {
                fprintf(stderr, "Could not load single test\n");
                goto error_parse_input;
            } else if (tests.length > 1) {
                fprintf(stderr, "Somehow loaded more than one tests when only running one???\n");
                goto error_parse_input;
            }
        }
    }

    struct TdoArenaState state = tdo_arena_state_get(arena);
    struct TdoFile *files = (struct TdoFile*) test_files.data;
    for (size_t i = 0; i < test_files.length; i++) {
        tdo_arena_state_set(arena, state);

        files[i].library = tdo_dynamic_library_load(files[i].name.bytes);
        char const *err = tdo_dynamic_get_error(arena);
        if (err != NULL) fprintf(stderr, "%s\n", err);
    }
    tdo_arena_state_set(arena, state);

    if (args.single_test == NULL) {
        result = tdo_run_all(args, output, arena, tests);
    } else {
        struct TdoTest *test = tests.data;
        tdo_run_single(test, arena, status);
    }

    for (size_t i = 0; i < test_files.length; i++) {
        if (files[i].library == NULL) continue;
        tdo_dynamic_library_unload(files[i].library);
    }
    error_parse_input:
    if (output != stdout) fclose(output);
    error_open_output:
    if (input != stdin) fclose(input);
    error_open_input:
    if (status != NULL) fclose(status);
    error_open_status:
    tdo_arena_deinit(string_arena);
    error_init_string_arena:
    tdo_arena_deinit(arena);
    error_init_arena:
    error_parse_args:
    return result;
}
