#include "error.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define TDO_PROCESS_MAX ((size_t) 2048)

struct TdoArguments {
    size_t processes;
    bool debug_one;
    char const *test_file;
    char const *output;
};

enum TdoError tdo_arguments_parse(struct TdoArguments *args, int argc, char **argv) {
    enum TdoError result = TDO_ERROR_OK;
    *args = (struct TdoArguments) {
        .processes = 1,
        .debug_one = false,
        .test_file = NULL,
        .output = NULL,
    };

    if (argc < 1) return TDO_ERROR_ARG_FIRST;
    argc -= 1; argv += 1;

    while (argc > 0) {
        char const *s = argv[0];

        if (s[0] != '-') {
            // positional
            if (args->test_file == NULL) {
                args->test_file = s;
            } else {
                fprintf(stderr, "Recieved additional positional argument: '%s'\n", s);
                result = TDO_ERROR_ARG_PARSE;
            }
        } else {
            // flag
            if (strncmp(s, "-g", 3) == 0) {
                args->debug_one = true;
            } else if (strncmp(s, "-j", 2) == 0) {
                errno = 0;
                char *err;
                unsigned long threads = strtoul(s + 2, &err, 10);
                if (errno) {
                    perror("Could not parse amount of processes");
                    result = TDO_ERROR_ARG_PARSE;
                } else if (threads > TDO_PROCESS_MAX) {
                    fprintf(stderr, "Specified too many processes: %lu > %zu\n", threads, TDO_PROCESS_MAX);
                    result = TDO_ERROR_ARG_PARSE;
                } else if (*err != '\0') {
                    fprintf(stderr, "Could not parse amount of threads: '%s'\n", s + 2);
                    result = TDO_ERROR_ARG_PARSE;
                } else if (threads == 0) {
                    fprintf(stderr, "Amount of processes must be strictly positive, got zero\n");
                    result = TDO_ERROR_ARG_PARSE;
                } else {
                    args->processes = (size_t) threads;
                }
            } else if (strncmp(s, "-o", 3) == 0) {
                if (argc <= 1) {
                    fprintf(stderr, "Missing output file argument to '-o'\n");
                    result = TDO_ERROR_ARG_PARSE;
                } else {
                    argc -= 1; argv += 1;
                    args->output = argv[0];
                }
            } else {
                fprintf(stderr, "Unrecognized argument: '%s'\n", s);
                result = TDO_ERROR_ARG_PARSE;
            }
        }

        argc -= 1; argv += 1;
    }

    if (args->test_file == NULL) {
        fprintf(stderr, "Missing positional argument: test file\n");
        result = TDO_ERROR_ARG_PARSE;
    }

    return result;
}
