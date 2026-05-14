#ifndef TDO_ARGUMENTS_H
#define TDO_ARGUMENTS_H
#include "error.h"
#include <stdlib.h>
#include <stdbool.h>

#define TDO_PROCESS_MAX ((size_t) 2048)

enum TdoFormat {
    TDO_FORMAT_HUMAN,
    TDO_FORMAT_JSON,
};

enum TdoVerbosity {
    TDO_VERBOSITY_NONE,
    TDO_VERBOSITY_MINOR,
    TDO_VERBOSITY_MAJOR,
};

struct TdoArguments {
    size_t processes;
    float time_limit;
    char const *single_test;
    char const *test_file;
    char const *output;
    char const *internal_status;
    enum TdoFormat format;
    enum TdoVerbosity verbosity;
    bool overwrite;
    bool stop_on_first_error;
};

enum TdoError tdo_arguments_parse(struct TdoArguments *args, int argc, char **argv);

#endif
