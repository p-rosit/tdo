#include <stdio.h>
#include "../../src/arguments.h"

enum TdoError tdo_mock_tdo_arguments_parse(struct TdoArguments *args, int argc, char **argv) {
    enum TdoError result = tdo_arguments_parse(args, argc, argv);
    fprintf(stdout, "{\n");

    fprintf(stdout, "\t\"processes\": %zu,\n", args->processes);

    fprintf(stdout, "\t\"time_limit\": %f,\n", args->time_limit);

    fprintf(stdout, "\t\"single_test\": ");
    if (args->single_test == NULL) {
        fprintf(stdout, "null,\n");
    } else {
        fprintf(stdout, "\"%s\",\n", args->single_test);
    }

    fprintf(stdout, "\t\"test_file\": ");
    if (args->test_file == NULL) {
        fprintf(stdout, "null,\n");
    } else {
        fprintf(stdout, "\"%s\",\n", args->test_file);
    }

    fprintf(stdout, "\t\"output\": ");
    if (args->output == NULL) {
        fprintf(stdout, "null,\n");
    } else {
        fprintf(stdout, "\"%s\",\n", args->output);
    }

    fprintf(stdout, "\t\"internal_status\": ");
    if (args->internal_status == NULL) {
        fprintf(stdout, "null,\n");
    } else {
        fprintf(stdout, "\"%s\",\n", args->internal_status);
    }

    fprintf(stdout, "\t\"format\": ");
    {
        switch (args->format) {
            case TDO_FORMAT_HUMAN:
                fprintf(stdout, "\"human\",\n");
                goto format_checked;
            case TDO_FORMAT_JSON:
                fprintf(stdout, "\"json\",\n");
                goto format_checked;
        }
        fprintf(stderr, "Unknown format: %d\n", args->format);
        abort();
        format_checked:
        (void)NULL; // tag excepts expression
    }

    fprintf(stdout, "\t\"verbosity\": ");
    {
        switch (args->verbosity) {
            case TDO_VERBOSITY_NONE:
                fprintf(stdout, "\"none\",\n");
                goto verbosity_checked;
            case TDO_VERBOSITY_MINOR:
                fprintf(stdout, "\"minor\",\n");
                goto verbosity_checked;
            case TDO_VERBOSITY_MAJOR:
                fprintf(stdout, "\"major\",\n");
                goto verbosity_checked;
        }
        fprintf(stderr, "Unknown verbosity: %d\n", args->verbosity);
        abort();
        verbosity_checked:
        (void)NULL; // tag excepts expression
    }

    fprintf(stdout, "\t\"overwrite\": %s,\n", args->overwrite ? "true" : "false");

    fprintf(stdout, "\t\"stop_on_first_error\": %s\n", args->stop_on_first_error ? "true" : "false");

    fprintf(stdout, "}\n");
    exit(result);
}
