#include "error.h"
#include "arguments.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

enum TdoError tdo_arguments_parse(struct TdoArguments *args, int argc, char **argv) {
    enum TdoError result = TDO_ERROR_OK;
    *args = (struct TdoArguments) {
        .processes = 1,
        .time_limit = 5.0,
        .single_test = NULL,
        .test_file = NULL,
        .output = NULL,
        .overwrite = false,
        .internal_status = NULL,
        .format = TDO_FORMAT_HUMAN,
        .verbosity = TDO_VERBOSITY_NONE,
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
            if (strcmp(s, "-t") == 0) {
                if (argc <= 1) {
                    fprintf(stderr, "Missing test argument to '-t'\n");
                    result = TDO_ERROR_ARG_PARSE;
                } else {
                    argc -= 1; argv += 1;
                    args->single_test = argv[0];
                }
            } else if (strncmp(s, "-j", 2) == 0) {
                char const *job_str = NULL;
                if (strcmp(s, "-j") == 0) {
                    if (argc <= 1) {
                        fprintf(stderr, "Missing argument to '-j'\n");
                        result = TDO_ERROR_ARG_PARSE;
                        argc -= 1; argv += 1;
                        continue;
                    } else {
                        argc -= 1; argv += 1;
                        job_str = argv[0];
                    }
                } else {
                    job_str = s + 2;
                }

                errno = 0;
                char *err;
                unsigned long threads = strtoul(job_str, &err, 10);
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
            } else if (strcmp(s, "-o") == 0) {
                if (argc <= 1) {
                    fprintf(stderr, "Missing output file argument to '-o'\n");
                    result = TDO_ERROR_ARG_PARSE;
                } else {
                    argc -= 1; argv += 1;
                    args->output = argv[0];
                }
            } else if (strcmp(s, "-f") == 0) {
                args->overwrite = true;
            } else if (strcmp(s, "--internal-status") == 0) {
                if (argc <= 1) {
                    fprintf(stderr, "Missing status file\n");
                    result = TDO_ERROR_ARG_PARSE;
                } else {
                    argc -= 1; argv += 1;
                    args->internal_status = argv[0];
                }
            } else if (strcmp(s, "--timeout") == 0) {
                if (argc <= 1) {
                    fprintf(stderr, "Missing argument to '--timeout'\n");
                    result = TDO_ERROR_ARG_PARSE;
                } else {
                    argc -= 1; argv += 1;
                    char const *timeout_str = argv[0];

                    errno = 0;
                    char *err;
                    float timeout = strtof(timeout_str, &err);
                    if (errno) {
                        perror("Could not parse timeout");
                        result = TDO_ERROR_ARG_PARSE;
                    } else if (*err != '\0') {
                        fprintf(stderr, "Could not parse timeout: '%s'\n", timeout_str);
                        result = TDO_ERROR_ARG_PARSE;
                    } else if (timeout <= 0) {
                        fprintf(stderr, "Timeout must be strictly positive, got %f\n", timeout);
                        result = TDO_ERROR_ARG_PARSE;
                    } else {
                        args->time_limit = timeout;
                    }
                }
            } else if (strcmp(s, "--format") == 0) {
                if (argc <= 1) {
                    fprintf(stderr, "Missing argument to '--format'\n");
                    result = TDO_ERROR_ARG_PARSE;
                } else {
                    argc -= 1; argv += 1;
                    char const *format_str = argv[0];

                    if (strcmp(format_str, "human") == 0) {
                        args->format = TDO_FORMAT_HUMAN;
                    } else if (strcmp(format_str, "json") == 0) {
                        args->format = TDO_FORMAT_JSON;
                    } else {
                        fprintf(stderr, "Unknown format argument '%s', see '-h' for options\n", format_str);
                        result = TDO_ERROR_ARG_PARSE;
                    }
                }
            } else if (strcmp(s, "-v") == 0) {
                args->verbosity = TDO_VERBOSITY_MINOR;
            } else if (strcmp(s, "-vv") == 0) {
                args->verbosity = TDO_VERBOSITY_MAJOR;
            } else {
                fprintf(stderr, "Unrecognized argument: '%s'\n", s);
                result = TDO_ERROR_ARG_PARSE;
            }
        }

        argc -= 1; argv += 1;
    }

    if (args->test_file != NULL && args->single_test != NULL) {
        fprintf(stderr, "Only specify an input file or a single test to run, not both\n");
        result = TDO_ERROR_ARG_PARSE;
    }

    return result;
}
