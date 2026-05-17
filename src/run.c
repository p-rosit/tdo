#define _XOPEN_SOURCE 600
#include "platform.h"
#include "str.h"
#include "arguments.h"
#include "run.h"
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(TDO_POSIX)
    #include "platform/run_posix.c"
#elif defined(TDO_WINDOWS)
    #include "platform/run_windows.c"
#else
    #error "Unknown platform"
#endif

void tdo_json_escaped(FILE *file, struct TdoString string) {
    for (size_t i = 0; i < string.length; i++) {
        unsigned char c = string.bytes[i];
        switch (string.bytes[i]) {
            case '\"': fputs("\\\"", file); break;
            case '\\': fputs("\\\\", file); break;
            case '\b': fputs("\\b", file); break;
            case '\f': fputs("\\f", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default:
                if (c < 0x20 || c > 0x7E) {
                    // control character or "random data"
                    fprintf(file, "\\u%04x", (unsigned int) c);
                } else {
                    // ascii
                    fputc(string.bytes[i], file); break;
                }
        }
    }
}

void tdo_human_escaped(FILE *file, struct TdoString string) {
    for (size_t i = 0; i < string.length; i++) {
        unsigned char c = string.bytes[i];
        switch (string.bytes[i]) {
            case '\"': fputs("\\\"", file); break;
            case '\\': fputs("\\\\", file); break;
            case '\b': fputs("\\b", file); break;
            case '\f': fputs("\\f", file); break;
            case '\n': fputs("\\n\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default:
                if (c < 0x20 || c > 0x7E) {
                    // control character or "random data"
                    fprintf(file, "\\u%04x", (unsigned int) c);
                } else {
                    // ascii
                    fputc(string.bytes[i], file); break;
                }
        }
    }
}

void tdo_log_dump(struct TdoLog log, FILE *file, char const *name) {
    fprintf(file, ",\n\t\t\"%s\": \"", name);
    tdo_json_escaped(file, log.data);
    fprintf(file, "\"");
}

void tdo_run_print_progress(FILE *file, struct TdoRunStatus *status) {
    bool warning = status->error > 0 || status->timeout > 0;
    bool error = status->signal > 0 || status->exit > 0 || status->stop > 0;

    if (!warning && !error) {
        fprintf(file, "[%3.0lf%%] ", 100.0 * (double) status->finished / (double) status->total);
    } else if (warning && !error) {
        tdo_colour_fprintf(file, TDO_COLOUR_BLUE, "[%3.0lf%%] ", 100.0 * (double) status->finished / (double) status->total);
    } else {
        tdo_colour_fprintf(file, TDO_COLOUR_RED, "[%3.0lf%%] ", 100.0 * (double) status->finished / (double) status->total);
    }
}

void tdo_run_report_exit(struct TdoArguments *args, struct TdoRunStatus *status, struct TdoRun *run, FILE *file, char const *step, TdoProcessStatus process_status, double duration, bool timed_out) {
    if (args->format == TDO_FORMAT_HUMAN) {
        bool log_output = false;

        if (!tdo_process_status_is_exit(process_status) || step[0] != 'f' || args->verbosity > TDO_VERBOSITY_NONE) {
            status->success_in_a_row = 0;
            if (status->finished > 0) fprintf(file, "\n");
            tdo_run_print_progress(file, status);
        } else if (status->finished == 0) {
            tdo_run_print_progress(file, status);
        }

        if (timed_out) {
            status->timeout += 1;
            status->any_failed = true;
            log_output = true;
            tdo_colour_fprintf(file, TDO_COLOUR_BLUE, "TIMEOUT");
        } else if (tdo_process_status_is_exit(process_status)) {
            if (step[0] == 'f') {
                if (status->finished > 0 && status->success_in_a_row == 0 && args->verbosity == TDO_VERBOSITY_NONE) {
                    fprintf(file, "\n");
                    tdo_run_print_progress(file, status);
                }

                status->success += 1;
                status->success_in_a_row += 1;

                if (args->verbosity == TDO_VERBOSITY_NONE) {
                    tdo_colour_fprintf(file, TDO_COLOUR_GREEN, ".");
                    if (status->success_in_a_row % 80 == 0) {
                        fprintf(file, "\n");
                        tdo_run_print_progress(file, status);
                    }
                } else {
                    tdo_colour_fprintf(file, TDO_COLOUR_GREEN, "SUCCESS");
                }
            } else {
                status->exit += 1;
                status->any_failed = true;
                log_output = true;
                tdo_colour_fprintf(file, TDO_COLOUR_RED, "UNEXPECTED EXIT");
                fprintf(file, " (" TDO_PROCESS_CODE_FORMAT ")", tdo_process_code_exit(process_status));
            }
        } else if (tdo_process_status_is_signal(process_status)) {
            status->signal += 1;
            status->any_failed = true;
            log_output = true;
            tdo_colour_fprintf(file, TDO_COLOUR_RED, "SIGNAL");
            fprintf(file, " (" TDO_PROCESS_CODE_FORMAT ")", tdo_process_code_signal(process_status));
        } else if (tdo_process_status_is_stop(process_status)) {
            status->stop += 1;
            status->any_failed = true;
            log_output = true;
            tdo_colour_fprintf(file, TDO_COLOUR_RED, "STOP");
            fprintf(file, " (" TDO_PROCESS_CODE_FORMAT ")", tdo_process_code_stop(process_status));
        }

        if (!tdo_process_status_is_exit(process_status) || step[0] != 'f' || args->verbosity > TDO_VERBOSITY_NONE) {
            fprintf(file, " test::%s::%s", run->test->symbol.file->name.bytes, run->test->symbol.name.bytes);
            struct TdoFixture *fixtures = run->test->fixtures.data;
            for (size_t i = 0; i < run->test->fixtures.length; i++) {
                struct TdoFixture *fixture = &fixtures[i];
                if (fixture->kind == TDO_FIXTURE_BEFORE) {
                    fprintf(file, " before::");
                } else if (fixture->kind == TDO_FIXTURE_AFTER) {
                    fprintf(file, " after::");
                } else {
                    fprintf(stderr, "Unknown fixture kind: %d\n", fixture->kind);
                    fflush(NULL);
                    abort();
                }
                fprintf(file, "%s::%s", fixture->symbol.file->name.bytes, fixture->symbol.name.bytes);
            }

            if (args->verbosity == TDO_VERBOSITY_MAJOR || step[0] != 'f') fprintf(file, "\n");
            if (timed_out || step[0] != 'f') fprintf(file, "Current step: %s\n", step);
        }

        if (log_output || args->verbosity == TDO_VERBOSITY_MAJOR) {
            tdo_colour_fprintf(file, TDO_COLOUR_GREY, "Captured stdout ----------------------------------------------------------------\n");
            tdo_human_escaped(file, run->out.data);
            tdo_colour_fprintf(file, TDO_COLOUR_GREY, "Captured stderr ----------------------------------------------------------------\n");
            tdo_human_escaped(file, run->err.data);
            tdo_colour_fprintf(file, TDO_COLOUR_GREY, "--------------------------------------------------------------------------------");
        }

    } else if (args->format == TDO_FORMAT_JSON) {
        if (status->finished > 0) fprintf(file, ",");
        fprintf(file, "\n");
        fprintf(file, "\t{\n");

        fprintf(file, "\t\t\"file\": \"");
        tdo_json_escaped(file, run->test->symbol.file->name);
        fprintf(file, "\",\n");

        fprintf(file, "\t\t\"name\": \"");
        tdo_json_escaped(file, run->test->symbol.name);
        fprintf(file, "\",\n");

        fprintf(file, "\t\t\"duration\": %lf,\n", duration);

        fprintf(file, "\t\t\"status\": \"");
        if (timed_out) {
            status->timeout += 1;
            status->any_failed = true;
            fprintf(file, "timeout");
        } else if (tdo_process_status_is_exit(process_status)) {
            if (step[0] == 'f') {
                status->success += 1;
                fprintf(file, "complete");
            } else {
                status->exit += 1;
                status->any_failed = true;
                fprintf(file, "exit");
            }
        } else if (tdo_process_status_is_signal(process_status)) {
            status->signal += 1;
            status->any_failed = true;
            fprintf(file, "signal");
        } else if (tdo_process_status_is_stop(process_status)) {
            status->stop += 1;
            status->any_failed = true;
            fprintf(file, "stop");
        }
        fprintf(file, "\"");

        if (!timed_out) {
            if (tdo_process_status_is_exit(process_status) && step[0] != 'f') {
                fprintf(file, ",\n\t\t\"exit\": " TDO_PROCESS_CODE_FORMAT, tdo_process_code_exit(process_status));
            } else if (tdo_process_status_is_signal(process_status)) {
                fprintf(file, ",\n\t\t\"signal\": " TDO_PROCESS_CODE_FORMAT, tdo_process_code_signal(process_status));
            } else if (tdo_process_status_is_stop(process_status)) {
                fprintf(file, ",\n\t\t\"stop\": " TDO_PROCESS_CODE_FORMAT, tdo_process_code_stop(process_status));
            }
        }

        if (timed_out || step[0] != 'f') {
            fprintf(file, ",\n\t\t\"step\": \"");
            tdo_json_escaped(file, (struct TdoString) { .length=strlen(step), .bytes=(char*)step });
            fprintf(file, "\"");
        }

        tdo_log_dump(run->out, file, "stdout");
        tdo_log_dump(run->err, file, "stderr");

        fprintf(file, "\n\t}");
    } else {
        fprintf(stderr, "Unknown format\n");
        fflush(NULL);
        abort();
    }
}

void tdo_run_report_error(struct TdoArguments *args, struct TdoRunStatus *status, struct TdoTest test, FILE *file, char const *step, char const *error, double duration) {
    status->error += 1;
    status->any_failed = true;

    if (args->format == TDO_FORMAT_HUMAN) {
        if (status->finished > 0) fprintf(file, "\n");
        tdo_run_print_progress(file, status);
        tdo_colour_fprintf(file, TDO_COLOUR_BLUE, "ERROR");
        fprintf(file, " test::%s::%s\n", test.symbol.file->name.bytes, test.symbol.name.bytes);
        struct TdoFixture *fixtures = test.fixtures.data;
        for (size_t i = 0; i < test.fixtures.length; i++) {
            struct TdoFixture *fixture = &fixtures[i];
            if (fixture->kind == TDO_FIXTURE_BEFORE) {
                fprintf(file, " before::");
            } else if (fixture->kind == TDO_FIXTURE_AFTER) {
                fprintf(file, " after::");
            } else {
                fprintf(stderr, "Unknown fixture kind: %d\n", fixture->kind);
                fflush(NULL);
                abort();
            }
            fprintf(file, "%s::%s", fixture->symbol.file->name.bytes, fixture->symbol.name.bytes);
        }
        fprintf(file, "Current step: %s\n", step);
        fprintf(file, "    %s", error);
    } else if (args->format == TDO_FORMAT_JSON) {
        if (status->finished > 0) fprintf(file, ",");
        fprintf(file, "\n");
        fprintf(file, "\t{\n");

        fprintf(file, "\t\t\"file\": \"");
        tdo_json_escaped(file, test.symbol.file->name);
        fprintf(file, "\",\n");

        fprintf(file, "\t\t\"name\": \"");
        tdo_json_escaped(file, test.symbol.name);
        fprintf(file, "\",\n");

        fprintf(file, "\t\t\"duration\": %lf,\n", duration);

        fprintf(file, "\t\t\"status\": \"error\",\n");
        fprintf(file, "\t\t\"error\": \"");
        tdo_json_escaped(file, (struct TdoString) { .length=strlen(error), .bytes=(char*)error });
        fprintf(file, "\",\n");

        fprintf(file, "\t\t\"step\": ");
        if (step != NULL) {
            fprintf(file, "\"");
            tdo_json_escaped(file, (struct TdoString) { .length=strlen(step), .bytes=(char*)step });
            fprintf(file, "\"\n");
        } else {
            fprintf(file, "null\n");
        }

        fprintf(file, "\t}");
    } else {
        fprintf(stderr, "Unknown format\n");
        fflush(NULL);
        abort();
    }
}

enum TdoError tdo_string_previous_line(struct TdoString *line, struct TdoString string, size_t index) {
    if (string.bytes == NULL || string.length == 0) return TDO_ERROR_EOF;
    if (string.bytes[index] != '\n') return TDO_ERROR_NEWLINE;

    size_t i;
    for (i = index - 1; 0 < i; --i) {
        if (string.bytes[i] == '\n') {
            i++;
            break;
        }
    }

    *line = (struct TdoString) {
        .bytes = &string.bytes[i],
        .length = index - i,
    };

    return TDO_ERROR_OK;
}

enum TdoError tdo_parse_size_t(size_t *number, char const *string) {
    if (string == NULL || *string == '\0') return TDO_ERROR_EOF;

    // strtoul ignores leading whitespace but if the string is just spaces,
    // strtoul won't set an error.
    while (isspace((unsigned char)*string)) string++;
    if (*string == '\0') return TDO_ERROR_EOF;

    // strtoul actually allows "-" we only have positive numbers
    if (*string == '-') return TDO_ERROR_NEGATIVE;

    // unsigned long long is guaranteed to be at least 64 bits, it's not
    // like anyone will have more fixtures than that...
    errno = 0;
    char *endptr;
    unsigned long long val = strtoull(string, &endptr, 10);
    if (errno == ERANGE) return TDO_ERROR_NUMBER;

    // did it even read anything?
    if (string == endptr) return TDO_ERROR_NUMBER;

    // did it read all of it?
    while (*endptr != '\0') {
        if (!isspace((unsigned char)*endptr)) return TDO_ERROR_NUMBER;
        endptr++;
    }

    // what if if `ULLONG_MAX > SIZE_MAX`?
    if (val > SIZE_MAX) return TDO_ERROR_NUMBER;

    *number = (size_t)val;
    return TDO_ERROR_OK;
}

enum TdoError tdo_run_report_assemble_step(struct TdoString *step, struct TdoArena *arena, struct TdoString step_name, struct TdoSymbol symbol) {
    *step = tdo_string_init();
    if (step_name.length >= 2 && step_name.bytes[0] == 'a' && step_name.bytes[1] == '_') {
        if (!tdo_string_append(step, arena, 5, "after")) return TDO_ERROR_MEMORY;
    } else if (step_name.length >= 2 && step_name.bytes[0] == 'b' && step_name.bytes[1] == '_') {
        if (!tdo_string_append(step, arena, 6, "before")) return TDO_ERROR_MEMORY;
    } else if (step_name.length >= 1 && step_name.bytes[0] == 't') {
        if (!tdo_string_append(step, arena, 4, "test")) return TDO_ERROR_MEMORY;
    } else {
        return TDO_ERROR_UNKNOWN;
    }
    if (!tdo_string_append(step, arena, 2, "::")) return TDO_ERROR_MEMORY;
    if (!tdo_string_append(step, arena, symbol.file->name.length, symbol.file->name.bytes)) return TDO_ERROR_MEMORY;
    if (!tdo_string_append(step, arena, 2, "::")) return TDO_ERROR_MEMORY;
    if (!tdo_string_append(step, arena, symbol.name.length, symbol.name.bytes)) return TDO_ERROR_MEMORY;
    return TDO_ERROR_OK;
}

void tdo_run_report_status(struct TdoArguments *args, struct TdoRunStatus *status, struct TdoRun *run, struct TdoArena *arena, FILE *file, TdoProcessStatus process_status, double duration, bool timed_out) {
    struct TdoArenaState state = tdo_arena_state_get(arena);

    struct TdoString log_status = run->status.data;
    if (log_status.bytes == NULL || log_status.length == 0) {
        tdo_run_report_error(args, status, *run->test, file, NULL, "no data in status pipe", duration);
        goto done;
    } else if (log_status.bytes[log_status.length - 1] != '\n') {
        tdo_run_report_error(args, status, *run->test, file, NULL, "malformed status pipe, does not end with newline", duration);
        goto done;
    } else if (log_status.length <= 1) {
        tdo_run_report_error(args, status, *run->test, file, NULL, "malformed status pipe, only contains newline", duration);
        goto done;
    }

    struct TdoString last_line = tdo_string_init();
    enum TdoError err = tdo_string_previous_line(&last_line, run->status.data, run->status.data.length - 1);
    if (err != TDO_ERROR_OK || last_line.length <= 0) {
        tdo_run_report_error(args, status, *run->test, file, NULL, "malformed status pipe, could not find last line", duration);
        goto done;
    }
    last_line.bytes[last_line.length] = '\0'; // replace newline with null terminator

    if (last_line.bytes[0] == 'e') {
        // got error while running

        struct TdoString step_name;
        err = tdo_string_previous_line(&step_name, run->status.data, run->status.data.length - last_line.length - 2);
        if (err != TDO_ERROR_OK || last_line.length <= 0) {
            tdo_run_report_error(args, status, *run->test, file, NULL, "malformed status pipe, could not find line before error", duration);
            goto done;
        }
        step_name.bytes[step_name.length] = '\0'; // overwrite newline

        struct TdoSymbol *current = NULL;
        if (step_name.bytes[0] == 't') {
            current = &run->test->symbol;
        } else if (step_name.length >= 2 && (step_name.bytes[0] == 'b' || step_name.bytes[0] == 'a') && step_name.bytes[1] == '_') {
            size_t index;
            enum TdoError err_parse = tdo_parse_size_t(&index, step_name.bytes + 2);
            if (err_parse != TDO_ERROR_OK) {
                tdo_run_report_error(args, status, *run->test, file, NULL, "malformed status pipe, could not parse fixture index", duration);
                goto done;
            }

            struct TdoFixture *fixtures = run->test->fixtures.data;
            for (size_t i = 0, fixture_index = 0; i < run->test->fixtures.length; i++) {
                struct TdoFixture *fixture = &fixtures[i];
                if ((step_name.bytes[0] == 'b' && fixture->kind == TDO_FIXTURE_BEFORE) || (step_name.bytes[0] == 'a' && fixture->kind == TDO_FIXTURE_AFTER)) {
                    if (fixture_index == index) {
                        current = &fixture->symbol;
                        break;
                    }
                    fixture_index += 1;
                }
            }

            if (current == NULL) {
                tdo_run_report_error(args, status, *run->test, file, step_name.bytes, "invalid current fixture index", duration);
                goto done;
            }
        } else {
            tdo_run_report_error(args, status, *run->test, file, step_name.bytes, "unknown error", duration);
            goto done;
        }

        struct TdoString step;
        enum TdoError err_step = tdo_run_report_assemble_step(&step, arena, step_name, *current);
        if (err_step != TDO_ERROR_OK) {
            tdo_run_report_error(args, status, *run->test, file, NULL, "could not build step", duration);
            goto done;
        }

        tdo_run_report_error(args, status, *run->test, file, step.bytes, last_line.bytes + 1, duration);
    } else if (last_line.length >= 2 && (last_line.bytes[0] == 'b' || last_line.bytes[0] == 'a') && last_line.bytes[1] == '_') {
        // unexpected exit while running fixture
        size_t index;
        enum TdoError err_parse = tdo_parse_size_t(&index, last_line.bytes + 2);
        if (err_parse != TDO_ERROR_OK) {
            tdo_run_report_error(args, status, *run->test, file, NULL, "malformed status pipe, could not parse fixture index", duration);
            goto done;
        }

        struct TdoSymbol *current = NULL;
        struct TdoFixture *fixtures = run->test->fixtures.data;
        for (size_t i = 0, fixture_index = 0; i < run->test->fixtures.length; i++) {
            struct TdoFixture *fixture = &fixtures[i];
            if ((last_line.bytes[0] == 'b' && fixture->kind == TDO_FIXTURE_BEFORE) || (last_line.bytes[0] == 'a' && fixture->kind == TDO_FIXTURE_AFTER)) {
                if (fixture_index == index) {
                    current = &fixture->symbol;
                    break;
                }
                fixture_index += 1;
            }
        }
        if (current == NULL) {
            tdo_run_report_error(args, status, *run->test, file, last_line.bytes, "invalid current fixture index", duration);
            goto done;
        }

        struct TdoString step;
        enum TdoError err_step = tdo_run_report_assemble_step(&step, arena, last_line, *current);
        if (err_step != TDO_ERROR_OK) {
            tdo_run_report_error(args, status, *run->test, file, NULL, "could not build step", duration);
            goto done;
        }

        tdo_run_report_exit(args, status, run, file, step.bytes, process_status, duration, timed_out);
    } else if (strncmp(last_line.bytes, "test", 4) == 0) {
        struct TdoString step;
        enum TdoError err_step = tdo_run_report_assemble_step(&step, arena, last_line, run->test->symbol);
        if (err_step != TDO_ERROR_OK) {
            tdo_run_report_error(args, status, *run->test, file, NULL, "could not build step", duration);
            goto done;
        }

        // unexpected exit while running test
        tdo_run_report_exit(args, status, run, file, step.bytes, process_status, duration, timed_out);
    } else if (strncmp(last_line.bytes, "finished", 8) == 0) {
        // test finished normally
        tdo_run_report_exit(args, status, run, file, last_line.bytes, process_status, duration, timed_out);
    } else {
        // unknown status
        tdo_run_report_error(args, status, *run->test, file, NULL, "unknown error", duration);
    }

    done:
    tdo_arena_state_set(arena, state);
    return;
}

void tdo_status_error(FILE *file, char const *fmt, ...) {
    FILE *f = file;
    if (file == NULL) {
        f = stderr;
    } else {
        fprintf(f, "e");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
}

void tdo_assert_library_loaded(struct TdoFile *file, FILE *status) {
    if (file->library == NULL) {
        tdo_status_error(status, "Could not load library: %s\n", file->name.bytes);
        fflush(NULL);
        abort();
    }
}

TdoTestSymbol *tdo_symbol_get(struct TdoSymbol symbol, struct TdoArena *arena, char const *name, FILE *status) {
    struct TdoArenaState state = tdo_arena_state_get(arena);

    void (*s)(void) = tdo_dynamic_symbol_load(symbol.file->library, symbol.name.bytes);
    char const *err = tdo_dynamic_get_error(arena);
    if (err != NULL) {
        tdo_status_error(status, "Could not load %s: %s\n", name, err);
        fflush(NULL);
        abort();
    } else if (s == NULL) {
        tdo_status_error(status, "Symbol is null\n");
        fflush(NULL);
        abort();
    }
    tdo_arena_state_set(arena, state);

    return s;
}

void tdo_run_fixtures(struct TdoTest *test, enum TdoFixtureKind kind, struct TdoArena *arena, FILE *status) {
    struct TdoArenaState state = tdo_arena_state_get(arena);

    char prefix;
    switch (kind) {
        case TDO_FIXTURE_BEFORE: prefix = 'b'; break;
        case TDO_FIXTURE_AFTER: prefix = 'a'; break;
        default:
            tdo_status_error(status, "Invalid fixture kind: %d\n", kind);
            fflush(NULL);
            abort();
    }

    struct TdoFixture *fixtures = test->fixtures.data;
    for (size_t i = 0, index = 0; i < test->fixtures.length; i++) {
        tdo_arena_state_set(arena, state);

        struct TdoFixture fixture = fixtures[i];
        if (fixture.kind == kind) {
            if (status != NULL) fprintf(status, "%c_%zu\n", prefix, index++);

            tdo_assert_library_loaded(fixture.symbol.file, status);

            void (*fix)(void) = tdo_symbol_get(fixture.symbol, arena, "fixture", status);
            fflush(NULL);
            fix();
        }
    }
    tdo_arena_state_set(arena, state);
}

void tdo_run_single(struct TdoTest *test, struct TdoArena *arena, FILE *status) {
    // run before fixtures
    tdo_run_fixtures(test, TDO_FIXTURE_BEFORE, arena, status);

    // do the test
    if (status != NULL) fprintf(status, "test\n");

    tdo_assert_library_loaded(test->symbol.file, status);
    
    void (*t)(void) = tdo_symbol_get(test->symbol, arena, "test", status);
    fflush(NULL);
    t();
    
    // run after fixtures
    tdo_run_fixtures(test, TDO_FIXTURE_AFTER, arena, status);

    if (status != NULL) fprintf(status, "finished\n");
}

enum TdoError tdo_run_all(struct TdoArguments args, FILE *output, struct TdoArena *arena, struct TdoArray tests) {
    enum TdoError result = TDO_ERROR_UNKNOWN;
    struct TdoArenaState state = tdo_arena_state_get(arena);
    if (args.internal_status == NULL) fprintf(stderr, "Running %zu tests\n", tests.length);

    struct TdoRunStatus status;
    result = tdo_run_status_init(&status, arena, args);
    status.total = tests.length;
    if (result != TDO_ERROR_OK) goto error_setup;

    TdoMonotoneTime time_start = tdo_time_get();

    if (args.format == TDO_FORMAT_JSON) fprintf(output, "[");

    while (status.finished < tests.length) {
        while (status.running < args.processes && status.started < tests.length && !status.fork_failed && !status.log_setup_failed) {
            fflush(stdout);
            fflush(output);
            fflush(stderr);

            struct TdoTest *test = &((struct TdoTest*) tests.data)[status.started];
            struct TdoRun *run = tdo_run_new(args.processes, status.runs);
            if (run == NULL) {
                fprintf(stderr, "could not start new test run, all test runs are active\n");
                fflush(NULL);
                abort();
            }
            status.started += 1;

            tdo_run_start_new(&status, arena, args, output, test, run);
        }

        tdo_run_poll_event(&status, arena, args, output, tests);

        if (status.running == 0 && status.fork_failed) {
            struct TdoTest *ts = tests.data;
            for (size_t i = status.started; i < tests.length; i++) {
                tdo_run_report_error(&args, &status, ts[i], output, NULL, "could not create child process", -1.0);
                status.finished += 1;
            }
        } else if (status.running == 0 && status.log_setup_failed) {
            struct TdoTest *ts = tests.data;
            for (size_t i = status.started; i < tests.length; i++) {
                tdo_run_report_error(&args, &status, ts[i], output, NULL, "could not setup log redirection", -1.0);
                status.finished += 1;
            }
        }

        if (args.stop_on_first_error && status.any_failed) {
            break;
        }
    }

    if (args.format == TDO_FORMAT_HUMAN) {
        fprintf(output, "\n");
    } else if (args.format == TDO_FORMAT_JSON) {
        fprintf(output, "\n]\n");
    } else {
        fprintf(stderr, "Unknown format\n");
        fflush(NULL);
        abort();
    }
    fflush(output);
    result = TDO_ERROR_OK;

    TdoMonotoneTime time_end = tdo_time_get();

    char const *spacing = "    ";

    if (status.finished < tests.length) {
        fprintf(stderr, "Stopped after running %zu/%zu tests in %.2lf seconds:\n", status.finished, tests.length, tdo_time_between(time_end, time_start));
    } else {
        fprintf(stderr, "Ran %zu tests in %.2lf seconds:\n", tests.length, tdo_time_between(time_end, time_start));
    }
    fprintf(stderr, "%ssuccess: ", spacing);
    tdo_colour_fprintf(stderr, TDO_COLOUR_GREEN, "%3zu/%zu\n", status.success, tests.length);

    size_t total_fails = status.exit + status.timeout + status.signal + status.error + status.stop;
    if (total_fails) {
        fprintf(stderr, "%sfailure: ", spacing);
        tdo_colour_fprintf(stderr, TDO_COLOUR_RED, "%3zu/%zu\n", total_fails, tests.length);

        if (status.exit > 0) {
            fprintf(stderr, "%s%sexit:    ", spacing, spacing);
            tdo_colour_fprintf(stderr, TDO_COLOUR_RED, "%3zu/%zu\n", status.exit, total_fails);
        }
        if (status.signal > 0) {
            fprintf(stderr, "%s%ssignal:  ", spacing, spacing);
            tdo_colour_fprintf(stderr, TDO_COLOUR_RED, "%3zu/%zu\n", status.signal, total_fails);
        }
        if (status.stop > 0) {
            fprintf(stderr, "%s%sstop:    ", spacing, spacing);
            tdo_colour_fprintf(stderr, TDO_COLOUR_RED, "%3zu/%zu\n", status.stop, total_fails);
        }
        if (status.timeout > 0) {
            fprintf(stderr, "%s%stimeout: ", spacing, spacing);
            tdo_colour_fprintf(stderr, TDO_COLOUR_BLUE, "%3zu/%zu\n", status.timeout, total_fails);
        }
        if (status.error > 0) {
            fprintf(stderr, "%s%serror:   ", spacing, spacing);
            tdo_colour_fprintf(stderr, TDO_COLOUR_BLUE, "%3zu/%zu\n", status.error, total_fails);
        }
    }

    tdo_run_status_deinit(status, args);
    error_setup:
    tdo_arena_state_set(arena, state);
    return result;
}
