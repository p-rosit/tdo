#ifndef TDO_ARGUMENTS_H
#define TDO_ARGUMENTS_H
#include "error.h"
#include <stdlib.h>
#include <stdbool.h>

#define TDO_PROCESS_MAX ((size_t) 2048)

struct TdoArguments {
    size_t processes;
    float time_limit;
    char const *single_test;
    char const *test_file;
    char const *output;
    char const *internal_status;
    bool overwrite;
};

enum TdoError tdo_arguments_parse(struct TdoArguments *args, int argc, char **argv);

#endif
