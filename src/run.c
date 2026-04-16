#include "error.h"
#include "arguments.c"
#include "test.c"
#include <string.h>

#if defined(TDO_WINDOWS)
    struct TdoOverlap {
        OVERLAPPED overlapped;
        struct TdoRun *run;
        enum { TDO_PIPE_WAITING, TDO_PIPE_CONNECTED, TDO_PIPE_CLOSED } status;
        enum { TDO_LOG_ERR, TDO_LOG_OUT, TDO_LOG_STATUS } kind;
        char buffer[1024];
    };
#endif

struct TdoRun {
    struct TdoTest *test;
    struct TdoLog out;
    struct TdoLog err;
    struct TdoLog status;
    TdoMonotoneTime start_time;

    #if defined(TDO_POSIX)
        pid_t pid;
    #elif defined(TDO_WINDOWS)
        DWORD pid;
        HANDLE process_handle;
        struct TdoOverlap out_ov;
        struct TdoOverlap err_ov;
        struct TdoOverlap status_ov;
    #else
        #error "Unknown platform"
    #endif

    bool active;
};

struct TdoRun *tdo_run_new(size_t length, struct TdoRun *runs) {
    for (size_t i = 0; i < length; i++) {
        if (!runs[i].active) return &runs[i];
    }
    return NULL;
}

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

void tdo_log_dump(struct TdoLog log, FILE *file, char const *name) {
    fprintf(file, ",\n\t\t\"%s\": \"", name);
    tdo_json_escaped(file, log.data);
    fprintf(file, "\"");
}

void tdo_run_report_exit(struct TdoRun run, FILE *file, char const *step, TdoProcessStatus status, double duration) {
    fprintf(file, "\n");
    fprintf(file, "\t{\n");

    fprintf(file, "\t\t\"file\": \"");
    tdo_json_escaped(file, run.test->symbol.file->name);
    fprintf(file, "\",\n");

    fprintf(file, "\t\t\"name\": \"");
    tdo_json_escaped(file, run.test->symbol.name);
    fprintf(file, "\",\n");

    fprintf(file, "\t\t\"duration\": %lf,\n", duration);

    fprintf(file, "\t\t\"status\": \"");
    if (tdo_process_status_is_exit(status)) {
        if (step[0] == 'f') {
            fprintf(file, "complete");
        } else {
            fprintf(file, "exit");
        }
    } else if (tdo_process_status_is_signal(status)) {
        fprintf(file, "signal");
    } else if (tdo_process_status_is_stop(status)) {
        fprintf(file, "stop");
    }
    fprintf(file, "\"");

    if (tdo_process_status_is_exit(status) && step[0] != 'f') {
        fprintf(file, ",\n\t\t\"exit\": " TDO_PROCESS_CODE_FORMAT, tdo_process_code_exit(status));
    } else if (tdo_process_status_is_signal(status)) {
        fprintf(file, ",\n\t\t\"signal\": " TDO_PROCESS_CODE_FORMAT, tdo_process_code_signal(status));
    } else if (tdo_process_status_is_stop(status)) {
        fprintf(file, ",\n\t\t\"stop\": " TDO_PROCESS_CODE_FORMAT, tdo_process_code_stop(status));
    }

    if (step[0] != 'f') {
        fprintf(file, ",\n\t\t\"step\": \"");
        tdo_json_escaped(file, (struct TdoString) { .length=strlen(step), .bytes=(char*)step });
        fprintf(file, "\"", step);
    }

    tdo_log_dump(run.out, file, "stdout");
    tdo_log_dump(run.err, file, "stderr");

    fprintf(file, "\n\t}");
}

void tdo_run_report_error(struct TdoTest test, FILE *file, char const *step, char const *error, double duration) {
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
        fprintf(file, "\"\n", step);
    } else {
        fprintf(file, "null\n");
    }

    fprintf(file, "\t}");
}

struct TdoRunStatus {
    struct TdoRun *runs;

    #if defined(TDO_POSIX)
        struct pollfd *fds;
        size_t *fd_to_idx;
    #elif defined(TDO_WINDOWS)
        HANDLE job;
        HANDLE iocp;
        LARGE_INTEGER clock_frequency;
        DWORD pid;
        struct TdoString executable_name;
    #else
        #error "Unknown platform"
    #endif

    size_t started;
    size_t finished;
    size_t running;
    bool fork_failed;
    bool log_setup_failed;
};

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

void tdo_run_report_status(struct TdoRun run, struct TdoArena *arena, FILE *file, int status, double duration) {
    struct TdoArenaState state = tdo_arena_state_get(arena);

    struct TdoString log_status = run.status.data;
    if (log_status.bytes == NULL || log_status.length == 0) {
        tdo_run_report_error(*run.test, file, NULL, "no data in status pipe", duration);
        goto done;
    } else if (log_status.bytes[log_status.length - 1] != '\n') {
        tdo_run_report_error(*run.test, file, NULL, "malformed status pipe, does not end with newline", duration);
        goto done;
    } else if (log_status.length <= 1) {
        tdo_run_report_error(*run.test, file, NULL, "malformed status pipe, only contains newline", duration);
        goto done;
    }

    struct TdoString last_line = tdo_string_init();
    enum TdoError err = tdo_string_previous_line(&last_line, run.status.data, run.status.data.length - 1);
    if (err != TDO_ERROR_OK || last_line.length <= 0) {
        tdo_run_report_error(*run.test, file, NULL, "malformed status pipe, could not find last line", duration);
        goto done;
    }
    last_line.bytes[last_line.length] = '\0'; // replace newline with null terminator

    if (last_line.bytes[0] == 'e') {
        // got error while running

        struct TdoString step_name;
        enum TdoError err = tdo_string_previous_line(&step_name, run.status.data, run.status.data.length - last_line.length - 2);
        if (err != TDO_ERROR_OK || last_line.length <= 0) {
            tdo_run_report_error(*run.test, file, NULL, "malformed status pipe, could not find line before error", duration);
            goto done;
        }
        step_name.bytes[step_name.length] = '\0'; // overwrite newline

        struct TdoSymbol *current = NULL;
        if (step_name.bytes[0] == 't') {
            current = &run.test->symbol;
        } else if (step_name.length >= 2 && (step_name.bytes[0] == 'b' || step_name.bytes[0] == 'a') && step_name.bytes[1] == '_') {
            size_t index;
            enum TdoError err_parse = tdo_parse_size_t(&index, step_name.bytes + 2);
            if (err_parse != TDO_ERROR_OK) {
                tdo_run_report_error(*run.test, file, NULL, "malformed status pipe, could not parse fixture index", duration);
                goto done;
            }

            struct TdoFixture *fixtures = run.test->fixtures.data;
            for (size_t i = 0, fixture_index = 0; i < run.test->fixtures.length; i++) {
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
                tdo_run_report_error(*run.test, file, step_name.bytes, "invalid current fixture index", duration);
                goto done;
            }
        } else {
            tdo_run_report_error(*run.test, file, step_name.bytes, "unknown error", duration);
            goto done;
        }

        struct TdoString step;
        enum TdoError err_step = tdo_run_report_assemble_step(&step, arena, step_name, *current);
        if (err_step != TDO_ERROR_OK) {
            tdo_run_report_error(*run.test, file, last_line.bytes, "could not build step", duration);
            goto done;
        }

        tdo_run_report_error(*run.test, file, step.bytes, last_line.bytes + 1, duration);
        goto done;
    }

    if (last_line.length >= 2 && (last_line.bytes[0] == 'b' || last_line.bytes[0] == 'a') && last_line.bytes[1] == '_') {
        // unexpected exit while running fixture
        size_t index;
        enum TdoError err_parse = tdo_parse_size_t(&index, last_line.bytes + 2);
        if (err_parse != TDO_ERROR_OK) {
            tdo_run_report_error(*run.test, file, NULL, "malformed status pipe, could not parse fixture index", duration);
            goto done;
        }

        struct TdoSymbol *current = NULL;
        struct TdoFixture *fixtures = run.test->fixtures.data;
        for (size_t i = 0, fixture_index = 0; i < run.test->fixtures.length; i++) {
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
            tdo_run_report_error(*run.test, file, last_line.bytes, "invalid current fixture index", duration);
            goto done;
        }

        struct TdoString step;
        enum TdoError err_step = tdo_run_report_assemble_step(&step, arena, last_line, *current);
        if (err_step != TDO_ERROR_OK) {
            tdo_run_report_error(*run.test, file, last_line.bytes, "could not build step", duration);
            goto done;
        }

        tdo_run_report_exit(run, file, step.bytes, status, duration);
    } else if (strncmp(last_line.bytes, "test", 4) == 0) {
        struct TdoString step;
        enum TdoError err_step = tdo_run_report_assemble_step(&step, arena, last_line, run.test->symbol);
        if (err_step != TDO_ERROR_OK) {
            tdo_run_report_error(*run.test, file, last_line.bytes, "could not build step", duration);
            goto done;
        }

        // unexpected exit while running test
        tdo_run_report_exit(run, file, step.bytes, status, duration);
    } else if (strncmp(last_line.bytes, "finished", 8) == 0) {
        // test finished normally
        tdo_run_report_exit(run, file, last_line.bytes, status, duration);
    } else {
        // unknown status
        tdo_run_report_error(*run.test, file, NULL, "unknown error", duration);
    }

    done:
    tdo_arena_state_set(arena, state);
    return;
}

void tdo_run_fixtures(struct TdoTest *test, enum TdoFixtureKind kind, struct TdoArena *arena, FILE *status) {
    struct TdoArenaState state = tdo_arena_state_get(arena);

    char prefix;
    switch (kind) {
        case TDO_FIXTURE_BEFORE: prefix = 'b'; break;
        case TDO_FIXTURE_AFTER: prefix = 'a'; break;
        default:
            if (status != NULL) {
                fprintf(status, "eInvalid fixture kind: %d\n", kind);
                fflush(status);
            } else {
                fprintf(stderr, "Invalid fixture kind: %d\n", kind);
                fflush(stderr);
            }
            abort();
    }

    struct TdoFixture *fixtures = test->fixtures.data;
    for (size_t i = 0, index = 0; i < test->fixtures.length; i++) {
        tdo_arena_state_set(arena, state);

        struct TdoFixture fixture = fixtures[i];
        if (fixture.kind == kind) {
            if (status != NULL) {
                fprintf(status, "%c_%zu\n", prefix, index++);
                fflush(status);
            }

            if (fixture.symbol.file->library == NULL) {
                if (status != NULL) {
                    fprintf(status, "eCould not load library: %s\n", fixture.symbol.file->name.bytes);
                    fflush(status);
                } else {
                    fprintf(stderr, "Could not load library: %s\n", fixture.symbol.file->name.bytes);
                    fflush(stderr);
                }
                abort();
            }

            void (*fix)(void) = tdo_dynamic_symbol_load(fixture.symbol.file->library, fixture.symbol.name.bytes);
            char const *err = tdo_dynamic_get_error(arena);
            if (err != NULL) {
                if (status != NULL) {
                    fprintf(status, "eCould not load fixture: %s\n", err);
                    fflush(status);
                } else {
                    fprintf(stderr, "Could not load fixture: %s\n", err);
                    fflush(stderr);
                }
                abort();
            } else if (fix == NULL) {
                if (status != NULL) {
                    fprintf(status, "eSymbol is null\n");
                    fflush(status);
                } else {
                    fprintf(stderr, "Symbol is null\n");
                    fflush(stderr);
                }
                abort();
            }
            fix();
        }
    }
    tdo_arena_state_set(arena, state);
}

void tdo_run_single(struct TdoTest *test, struct TdoArena *arena, FILE *status) {
    // run before fixtures
    tdo_run_fixtures(test, TDO_FIXTURE_BEFORE, arena, status);

    // do the test
    if (status != NULL) {
        fprintf(status, "test\n");
        fflush(status);
    }

    if (test->symbol.file->library == NULL) {
        if (status != NULL) {
            fprintf(status, "eCould not load library: %s\n", test->symbol.file->name.bytes);
            fflush(status);
        } else {
            fprintf(stderr, "Could not load library: %s\n", test->symbol.file->name.bytes);
            fflush(stderr);
        }
        abort();
    }
    
    void (*t)(void) = tdo_dynamic_symbol_load(test->symbol.file->library, test->symbol.name.bytes);
    struct TdoArenaState state = tdo_arena_state_get(arena);
    char const *err = tdo_dynamic_get_error(arena);
    if (err != NULL) {
        if (status != NULL) {
            fprintf(status, "eCould not load test: %s\n", err);
            fflush(status);
        } else {
            fprintf(stderr, "Could not load test: %s\n", err);
            fflush(stderr);
        }
        abort();
    } else if (t == NULL) {
        if (status != NULL) {
            fprintf(status, "eSymbol is null\n");
            fflush(status);
        } else {
            fprintf(stderr, "Symbol is null\n");
            fflush(stderr);
        }
        abort();
    }
    tdo_arena_state_set(arena, state);
    t();
    
    // run after fixtures
    tdo_run_fixtures(test, TDO_FIXTURE_AFTER, arena, status);

    if (status != NULL) {
        fprintf(status, "finished\n");
        fflush(status);
    }
}

#if defined(TDO_POSIX)
    size_t tdo_run_assemble_active_fds(struct pollfd *fds, size_t *fd_to_idx, size_t length, struct TdoRun *runs) {
        size_t fd_count = 0;
        for (size_t i = 0; i < length; i++) {
            struct TdoRun *run = &runs[i];
            if (run->active) {
                fds[fd_count].fd = run->out.fd;
                fds[fd_count].events = POLLIN;
                fd_to_idx[fd_count++] = i;
                fds[fd_count].fd = run->err.fd;
                fds[fd_count].events = POLLIN;
                fd_to_idx[fd_count++] = i;
                fds[fd_count].fd = run->status.fd;
                fds[fd_count].events = POLLIN;
                fd_to_idx[fd_count++] = i;
            }
        }
        return fd_count;
    }

    void tdo_run_start_new(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoTest *test, struct TdoRun *run) {
        int p_out[2], p_err[2], p_status[2];
        if (pipe(p_out) || pipe(p_err) || pipe(p_status)) {
            // we should possibly close the pipes we were able to open
            // here, but we're just about to exit anyway, so who cares
            status->log_setup_failed = true;
            status->started -= 1;
            return;
        }

        // set all pipes to be non-blocking
        {
            int flags = fcntl(p_out[0], F_GETFL, 0);
            fcntl(p_out[0], F_SETFL, flags | O_NONBLOCK);

            flags = fcntl(p_err[0], F_GETFL, 0);
            fcntl(p_err[0], F_SETFL, flags | O_NONBLOCK);

            flags = fcntl(p_status[0], F_GETFL, 0);
            fcntl(p_status[0], F_SETFL, flags | O_NONBLOCK);
        }

        // set all pipes to not be inherited by exec
        {
            fcntl(p_out[0], F_SETFD, FD_CLOEXEC);
            fcntl(p_out[1], F_SETFD, FD_CLOEXEC);
            fcntl(p_err[0], F_SETFD, FD_CLOEXEC);
            fcntl(p_err[1], F_SETFD, FD_CLOEXEC);
            fcntl(p_status[0], F_SETFD, FD_CLOEXEC);
            fcntl(p_status[1], F_SETFD, FD_CLOEXEC);
        }

        TdoMonotoneTime start_time = tdo_time_get();

        pid_t pid = fork();
        switch (pid) {
            case 0:
                // child

                // close output file
                if (output != stdout) fclose(output);

                // close all pipe fds inherited from parent's other active runs
                for (size_t i = 0; i < args.processes; i++) {
                    if (status->runs[i].active) {
                        close(status->runs[i].out.fd);
                        close(status->runs[i].err.fd);
                        close(status->runs[i].status.fd);
                    }
                }

                // close write end of input pipes
                close(p_out[0]); close(p_err[0]); close(p_status[0]);

                // replace stdout/stderr
                dup2(p_out[1], STDOUT_FILENO);
                dup2(p_err[1], STDERR_FILENO);

                tdo_run_single(test, arena, p_status[1]);

                // flush buffers
                fflush(stdout);
                fflush(stderr);
                _exit(0);
            case -1:
                status->fork_failed = true;
                status->started -= 1;
                return;
        }

        close(p_out[1]); close(p_err[1]); close(p_status[1]);
        run->active = true;
        run->test = test;
        run->start_time = start_time;
        run->pid = pid;
        tdo_log_reset(&run->out, p_out[0]);
        tdo_log_reset(&run->err, p_err[0]);
        tdo_log_reset(&run->status, p_status[0]);

        status->running += 1;
    }

    void tdo_run_poll_event(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoArray tests) {
        size_t fd_count = tdo_run_assemble_active_fds(status->fds, status->fd_to_idx, args.processes, status->runs);

        errno = 0;
        if (poll(status->fds, fd_count, 100) > 0) {
            for (size_t i = 0; i < fd_count; i++) {
                if (status->fds[i].revents & POLLIN) {
                    struct TdoRun *run = &status->runs[status->fd_to_idx[i]];

                    enum TdoError err;
                    if (status->fds[i].fd == run->out.fd) {
                        err = tdo_log_drain(&run->out, arena);
                    } else if (status->fds[i].fd == run->err.fd) {
                        err = tdo_log_drain(&run->err, arena);
                    } else {
                        err = tdo_log_drain(&run->status, arena);
                    }

                    if (err != TDO_ERROR_OK) {
                        run->active = false;
                        status->running -= 1;

                        TdoMonotoneTime end_time = tdo_time_get();
                        double duration = (
                            (double)(end_time.tv_sec - run->start_time.tv_sec)
                            + (double)(end_time.tv_nsec - run->start_time.tv_nsec) * 1e-9
                        );

                        if (status->finished > 0) fprintf(output, ",");
                        tdo_run_report_error(*run->test, output, NULL, "could not read output", duration);
                        status->finished += 1;
                        // TODO: kill process since it can't be read from? Barfed too much logs or something idk
                    }
                }
            }
        } else if (errno != 0 && errno != EINTR && errno != EAGAIN) {
            perror("poll failed");

            struct timespec end_time = tdo_time_get();

            for (size_t i = 0; i < args.processes; i++) {
                struct TdoRun *run = &status->runs[i];
                if (run->active) {
                    double duration = (
                        (double)(end_time.tv_sec - run->start_time.tv_sec)
                        + (double)(end_time.tv_nsec - run->start_time.tv_nsec) * 1e-9
                    );
                    tdo_run_report_error(*run->test, output, NULL, "could not poll pipes", duration);
                    status->finished += 1;
                    run->active = false;
                }
            }

            struct TdoTest *ts = tests.data;
            for (size_t i = status->started; i < tests.length; i++) {
                if (status->finished > 0) fprintf(output, ",");
                tdo_run_report_error(ts[i], output, NULL, "could not poll pipes", -1.0);
                status->finished += 1;
            }
        }
    }

    void tdo_run_poll_exit(struct TdoRun *run, struct TdoRunStatus *status, struct TdoArena *arena, FILE *output) {
        int return_status;
        if (waitpid(run->pid, &return_status, WNOHANG) > 0) {
            enum TdoError out_err = tdo_log_drain(&run->out, arena);
            enum TdoError err_err = tdo_log_drain(&run->err, arena);
            enum TdoError status_err = tdo_log_drain(&run->status, arena);

            struct timespec end_time = tdo_time_get();

            double duration = (
                (double)(end_time.tv_sec - run->start_time.tv_sec)
                + (double)(end_time.tv_nsec - run->start_time.tv_nsec) * 1e-9
            );

            if (status->finished > 0) fprintf(output, ",");
            if (out_err == TDO_ERROR_OK && err_err == TDO_ERROR_OK && status_err == TDO_ERROR_OK)  {
                tdo_run_report_status(*run, arena, output, return_status, duration);
            } else {
                tdo_run_report_error(*run->test, output, NULL, "could not read output", duration);
            }

            run->active = false;
            close(run->out.fd);
            close(run->err.fd);
            close(run->status.fd);
            status->running -= 1;
            status->finished += 1;
        }
    }
#elif defined(TDO_WINDOWS)
    // longest pipe name: \\.\pipe\tdo_pipe_AAAAAA_18446744073709551615_4294967296_18446744073709551615
    #define TDO_MAX_PIPE_NAME_LENGTH (78)

    enum TdoError tdo_run_unique_pipe_name(struct TdoString *name, size_t length, char *buffer, char const *prefix, size_t index, DWORD pid, ULONGLONG ticks) {
        errno = 0;
        int total_written = snprintf(buffer, length - 1, "\\\\.\\pipe\\tdo_pipe_%s_%zu_%lu_%llu", prefix, index, pid, ticks);
        if (total_written >= length) return TDO_ERROR_MEMORY; // buffer too small
        if (total_written < 0) {
            perror("Could not write unique name");
            return TDO_ERROR_UNKNOWN;
        }
        name->bytes = buffer;
        name->length = total_written;
        return TDO_ERROR_OK;
    }

    int tdo_run_pipes_pending(struct TdoRun *run) {
        return (
            (run->out_ov.status != TDO_PIPE_CLOSED)
            + (run->err_ov.status != TDO_PIPE_CLOSED)
            + (run->status_ov.status != TDO_PIPE_CLOSED)
        );
    }

    enum TdoError tdo_pipe_connect(struct TdoArena *arena, struct TdoRunStatus *status, struct TdoRun *run, struct TdoOverlap *overlap) {
        struct TdoLog *log;
        switch (overlap->kind) {
            case TDO_LOG_OUT: log = &run->out; break;
            case TDO_LOG_ERR: log = &run->err; break;
            case TDO_LOG_STATUS: log = &run->status; break;
            default: fprintf(stderr, "Invalid enum value: %d\n", overlap->kind); abort();
        }

        ZeroMemory(&overlap->overlapped, sizeof(overlap->overlapped));
        overlap->status = TDO_PIPE_WAITING;

        BOOL connected = ConnectNamedPipe(log->fd, &overlap->overlapped);
        DWORD code = GetLastError();
        if (connected || code == ERROR_PIPE_CONNECTED || code == ERROR_BROKEN_PIPE || code == ERROR_NO_DATA) {
            fprintf(stderr, "Unexpected connection while connecting pipe, got windows error code: %ld\n", code);
            fflush(stderr);
            abort();
        } else if (code == ERROR_IO_PENDING) {
            // waiting for IOCP packet
            // fprintf(stderr, "Not yet connected\n");
        } else {
            fprintf(stderr, "TODO1: fail gracefully %ld '%s'\n", code, tdo_dynamic_get_error(arena));
            abort();
        }

        return TDO_ERROR_OK;
    }

    void tdo_run_start_new(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoTest *test, struct TdoRun *run) {
        struct TdoArenaState state = tdo_arena_state_get(arena);
        // fprintf(stderr, "Starting test: '%s'\n", test->symbol.name.bytes);

        // open named pipes: stdout/stderr/status
        ULONGLONG ticks = GetTickCount64();
        
        struct TdoString out_name;
        char out_buffer[TDO_MAX_PIPE_NAME_LENGTH];
        if (tdo_run_unique_pipe_name(&out_name, sizeof(out_buffer), out_buffer, "out", status->running, status->pid, ticks) != TDO_ERROR_OK) {
            fprintf(stderr, "Failed to format out pipe name\n");
            abort();
        }

        struct TdoString err_name;
        char err_buffer[TDO_MAX_PIPE_NAME_LENGTH];
        if (tdo_run_unique_pipe_name(&err_name, sizeof(err_buffer), err_buffer, "err", status->running, status->pid, ticks) != TDO_ERROR_OK) {
            fprintf(stderr, "Failed to format err pipe name\n");
            abort();
        }
        
        struct TdoString status_name;
        char status_buffer[TDO_MAX_PIPE_NAME_LENGTH];
        if (tdo_run_unique_pipe_name(&status_name, sizeof(status_buffer), status_buffer, "status", status->running, status->pid, ticks) != TDO_ERROR_OK) {
            fprintf(stderr, "Failed to format status pipe name\n");
            abort();
        }

        SECURITY_ATTRIBUTES sa;
        ZeroMemory(&sa, sizeof(sa));
        // sa.cb = sizeof(sa);
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        
        SECURITY_ATTRIBUTES sa_pipes;
        ZeroMemory(&sa_pipes, sizeof(sa_pipes));
        sa_pipes.nLength = sizeof(sa_pipes);
        sa_pipes.bInheritHandle = FALSE;
        sa_pipes.lpSecurityDescriptor = NULL;

        HANDLE h_out = CreateNamedPipe(
            out_name.bytes,
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_WAIT,            // Byte mode, blocking
            1,                                     // Max instances
            1024,                                  // Out buffer
            1024,                                  // In buffer
            0,                                     // Default timeout
            &sa_pipes                              // Default security
        );
        if (GetLastError()) {
            fprintf(stderr, "Could not open pipe: %lu '%s'\n", GetLastError(), tdo_dynamic_get_error(arena));
            abort();
        }
        
        HANDLE h_err = CreateNamedPipe(
            err_name.bytes,
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_WAIT,            // Byte mode, blocking
            1,                                     // Max instances
            1024,                                  // Out buffer
            1024,                                  // In buffer
            0,                                     // Default timeout
            &sa_pipes                              // Default security
        );
        if (GetLastError()) {
            fprintf(stderr, "Could not open pipe: %lu '%s'\n", GetLastError(), tdo_dynamic_get_error(arena));
            abort();
        }
        
        HANDLE h_status = CreateNamedPipe(
            status_name.bytes,
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_WAIT,            // Byte mode, blocking
            1,                                     // Max instances
            1024,                                  // Out buffer
            1024,                                  // In buffer
            0,                                     // Default timeout
            &sa_pipes                              // Default security
        );
        if (GetLastError()) {
            fprintf(stderr, "Could not open pipe: %lu '%s'\n", GetLastError(), tdo_dynamic_get_error(arena));
            abort();
        }

        struct TdoString command = tdo_string_init();
        {
            bool could_build = true;
            if (!(could_build = tdo_string_append(&command, arena, status->executable_name.length, status->executable_name.bytes))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 1, " "))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 2, "-t"))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 1, " "))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 1, "\""))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 4, "test"))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 2, "::"))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, test->symbol.file->name.length, test->symbol.file->name.bytes))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 2, "::"))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, test->symbol.name.length, test->symbol.name.bytes))) goto error_build_command;

            struct TdoFixture *fixtures = test->fixtures.data;
            for (size_t i = 0; i < test->fixtures.length; i++) {
                struct TdoFixture fixture = fixtures[i];

                if (!(could_build = tdo_string_append(&command, arena, 1, " "))) goto error_build_command;

                switch (fixture.kind) {
                    case TDO_FIXTURE_BEFORE: if (!(could_build = tdo_string_append(&command, arena, 6, "before"))) goto error_build_command; break;
                    case TDO_FIXTURE_AFTER: if (!(could_build = tdo_string_append(&command, arena, 5, "after"))) goto error_build_command; break;
                    default: fprintf(stderr, "Invalid fixture kind: %d\n", fixture.kind); abort();
                }

                if (!(could_build = tdo_string_append(&command, arena, 2, "::"))) goto error_build_command;
                if (!(could_build = tdo_string_append(&command, arena, fixture.symbol.file->name.length, fixture.symbol.file->name.bytes))) goto error_build_command;
                if (!(could_build = tdo_string_append(&command, arena, 2, "::"))) goto error_build_command;
                if (!(could_build = tdo_string_append(&command, arena, fixture.symbol.name.length, fixture.symbol.name.bytes))) goto error_build_command;
            }
            
            if (!(could_build = tdo_string_append(&command, arena, 1, "\""))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 1, " "))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 17, "--internal-status"))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, 1, " "))) goto error_build_command;
            if (!(could_build = tdo_string_append(&command, arena, status_name.length, status_name.bytes))) goto error_build_command;
            
            error_build_command:
            if (!could_build) {
                status->fork_failed = true;
                status->running -= 1;
                fprintf(stderr, "Could not allocate command to start test\n");
                return;
            }
        }

        run->test = test;
        tdo_log_reset(&run->out, h_out);
        tdo_log_reset(&run->err, h_err);
        tdo_log_reset(&run->status, h_status);
        run->out_ov = (struct TdoOverlap) {
            .overlapped = {0},
            .run = run,
            .status = TDO_PIPE_WAITING,
            .kind = TDO_LOG_OUT,
        };
        run->err_ov = (struct TdoOverlap) {
            .overlapped = {0},
            .run = run,
            .status = TDO_PIPE_WAITING,
            .kind = TDO_LOG_ERR,
        };
        run->status_ov = (struct TdoOverlap) {
            .overlapped = {0},
            .run = run, 
            .status = TDO_PIPE_WAITING,
            .kind = TDO_LOG_STATUS,
        };

        // set up IOCP
        CreateIoCompletionPort(h_out, status->iocp, (ULONG_PTR)&run->out_ov, 0);
        CreateIoCompletionPort(h_err, status->iocp, (ULONG_PTR)&run->err_ov, 0);
        CreateIoCompletionPort(h_status, status->iocp, (ULONG_PTR)&run->status_ov, 0);

        // Start first read
        enum TdoError err_read = tdo_pipe_connect(arena, status, run, &run->out_ov);
        if (err_read != TDO_ERROR_OK) {
            fprintf(stderr, "Could not start reading from child\n");
            status->started -= 1;
            return;
        }

        err_read = tdo_pipe_connect(arena, status, run, &run->err_ov);
        if (err_read != TDO_ERROR_OK) {
            fprintf(stderr, "Could not start reading from child\n");
            status->started -= 1;
            return;
        }

        err_read = tdo_pipe_connect(arena, status, run, &run->status_ov);
        if (err_read != TDO_ERROR_OK) {
            fprintf(stderr, "Could not start reading from child\n");
            status->started -= 1;
            return;
        }
        
        HANDLE h_out_client = CreateFile(
            out_name.bytes,
            GENERIC_WRITE,
            0,
            &sa, // Must be inheritable
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, // <--- Synchronous!
            NULL
        );
        
        HANDLE h_err_client = CreateFile(
            err_name.bytes,
            GENERIC_WRITE,
            0,
            &sa, // Must be inheritable
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, // <--- Synchronous!
            NULL
        );

        STARTUPINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = INVALID_HANDLE_VALUE;
        si.hStdOutput = h_out_client;
        si.hStdError = h_err_client;

        PROCESS_INFORMATION pi;
        TdoMonotoneTime start_time = tdo_time_get();
        if (!CreateProcess(NULL, command.bytes, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            fprintf(stderr, "Could not start process...\n");
            status->fork_failed = true;
            status->started -= 1;
            return;
        }
        CloseHandle(pi.hThread);
        CloseHandle(h_out_client);
        CloseHandle(h_err_client);

        run->start_time = start_time;
        run->pid = pi.dwProcessId;
        run->process_handle = pi.hProcess;

        if (!AssignProcessToJobObject(status->job, pi.hProcess)) {
            fprintf(stderr, "Could not assign process to job\n");
            status->fork_failed = true;
            status->started -= 1;
            TerminateProcess(pi.hProcess, 1); // child was not tethered to parent
            CloseHandle(pi.hProcess);
            return;
        }

        run->active = true;
        status->running += 1;
    }

    void tdo_run_poll_event(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoArray tests) {
        DWORD bytes_transferred;
        ULONG_PTR completion_key;
        LPOVERLAPPED overlapped;

        if (GetQueuedCompletionStatus(status->iocp, &bytes_transferred, &completion_key, &overlapped, 100)) {
            struct TdoOverlap *ov = (struct TdoOverlap*) overlapped;
            struct TdoRun *run = ov->run;

            HANDLE pipe_handle;
            struct TdoLog *log;
            switch (ov->kind) {
                case TDO_LOG_ERR:
                    log = &run->err;
                    pipe_handle = run->err.fd;
                    break;
                case TDO_LOG_OUT:
                    log = &run->out;
                    pipe_handle = run->out.fd;
                    break;
                case TDO_LOG_STATUS:
                    log = &run->status;
                    pipe_handle = run->status.fd;
                    break;
                default:
                    fprintf(stderr, "Invalid log type\n");
                    abort();
            }

            if (overlapped != NULL && ov->status == TDO_PIPE_WAITING) {
                struct TdoOverlap *ov = (struct TdoOverlap*) overlapped;
                ov->status = TDO_PIPE_CONNECTED;

                BOOL success = ReadFile(pipe_handle, ov->buffer, sizeof(ov->buffer), NULL, &ov->overlapped);
                if (!success) {
                    DWORD code = GetLastError();
                    if (code == ERROR_IO_PENDING) {
                        // next read started
                    } else if (code == ERROR_BROKEN_PIPE || code == ERROR_PIPE_NOT_CONNECTED) {
                        ov->status = TDO_PIPE_CLOSED;
                    } else {
                        fprintf(stderr, "async ReadFile Failed: %lu '%s'\n", code, tdo_dynamic_get_error(arena));
                        abort();
                    }
                }

                return;
            }

            if (bytes_transferred > 0) {
                bool result = tdo_string_append(&log->data, arena, bytes_transferred, ov->buffer);
                if (!result) {
                    run->active = false;
                    status->running -= 1;

                    LARGE_INTEGER end_time = tdo_time_get();
                    double duration = (double)(end_time.QuadPart - run->start_time.QuadPart) / status->clock_frequency.QuadPart;

                    if (status->finished > 0) fprintf(output, ",");
                    tdo_run_report_error(*run->test, output, NULL, "could not read output", duration);
                    status->finished += 1;

                    TerminateProcess(run->process_handle, 1); // test produced more logs than we can read, why let it continue?
                    CloseHandle(run->process_handle);
                    return;
                }

                BOOL success = ReadFile(pipe_handle, ov->buffer, sizeof(ov->buffer), NULL, &ov->overlapped);
                if (!success) {
                    DWORD code = GetLastError();
                    if (code == ERROR_IO_PENDING) {
                        // next read started
                    } else if (code == ERROR_BROKEN_PIPE || code == ERROR_PIPE_NOT_CONNECTED) {
                        ov->status = TDO_PIPE_CLOSED;
                    } else {
                        fprintf(stderr, "async ReadFile Failed: %lu '%s'\n", code, tdo_dynamic_get_error(arena));
                        abort();
                    }
                }
            } else {
                // no bytes transferred, pipe closed
                ov->status = TDO_PIPE_CLOSED;
            }
            return;
        }

        DWORD code = GetLastError();
        if (overlapped != NULL) {
            struct TdoOverlap *ov = (struct TdoOverlap*) overlapped;

            if (code == ERROR_BROKEN_PIPE) {
                ov->status = TDO_PIPE_CLOSED;
            } else {
                fprintf(stderr, "Something went wrong! %lu '%s'\n", GetLastError(), tdo_dynamic_get_error(arena));
                abort();
            }
        } else if (code == 258) {
            // timed out
        } else {
            fprintf(stderr, "Something went wrong! %lu '%s'\n", GetLastError(), tdo_dynamic_get_error(arena));
            abort();
        }
    }

    void tdo_run_poll_exit(struct TdoRun *run, struct TdoRunStatus *status, struct TdoArena *arena, FILE *output) {
        if (tdo_run_pipes_pending(run) > 0) return;

        DWORD wait_status = WaitForSingleObject(run->process_handle, 0);

        if (wait_status == WAIT_TIMEOUT) {
            // test still running
        } else if (wait_status == WAIT_OBJECT_0) {
            DWORD return_status;
            if (!GetExitCodeProcess(run->process_handle, &return_status)) {
                fprintf(stderr, "Get exit code failed somehow... TODO: graceful exit\n");
                abort();
            }

            LARGE_INTEGER end_time = tdo_time_get();
            double duration = (double)(end_time.QuadPart - run->start_time.QuadPart) / status->clock_frequency.QuadPart;

            if (status->finished > 0) fprintf(output, ",");
            tdo_run_report_status(*run, arena, output, return_status, duration);

            run->active = false;
            CloseHandle(run->process_handle);
            CloseHandle(run->out.fd);
            CloseHandle(run->err.fd);
            CloseHandle(run->status.fd);
            status->running -= 1;
            status->finished += 1;
        } else {
            fprintf(stderr, "Wait for process failed somehow... TODO: graceful exit\n");
            abort();
        }
    }
#else
    #error "Unknown platform"
#endif

enum TdoError tdo_run_all(struct TdoArguments args, FILE *output, struct TdoArena *arena, struct TdoArray tests) {
    enum TdoError result = TDO_ERROR_UNKNOWN;
    struct TdoArenaState state = tdo_arena_state_get(arena);
    fprintf(stderr, "Running %zu tests\n", tests.length);

    struct TdoRunStatus status = {
        .runs = NULL,

        #if defined(TDO_POSIX)
            .fds = NULL,
            .fd_to_idx = NULL,
        #elif defined(TDO_WINDOWS)
            .pid = GetCurrentProcessId(),
            .job = NULL,
            .iocp = NULL,
            .clock_frequency = 1,
        #endif

        .started = 0,
        .finished = 0,
        .running = 0,
        .fork_failed = false,
        .log_setup_failed = false,
    };

    status.runs = tdo_arena_alloc(arena, sizeof(struct TdoRun), args.processes);
    if (status.runs == NULL) {
        result = TDO_ERROR_MEMORY;
        goto error_setup;
    }

    #if defined(TDO_POSIX)
        if (args.processes > SIZE_MAX / 3) return TDO_ERROR_MEMORY;
        status.fds = tdo_arena_alloc(arena, sizeof(struct pollfd), 3 * args.processes);
        if (status.fds == NULL) {
            result = TDO_ERROR_MEMORY;
            goto error_setup;
        }

        status.fd_to_idx = tdo_arena_alloc(arena, sizeof(size_t), 3 * args.processes);
        if (status.fd_to_idx == NULL) {
            result = TDO_ERROR_MEMORY;
            goto error_setup;
        }
    #elif defined(TDO_WINDOWS)
        char exe_name[MAX_PATH];
        DWORD length = GetModuleFileNameA(NULL, (LPSTR) &exe_name, sizeof(exe_name));
        if (length == 0) {
            fprintf(stderr, "Could not get executable name: %lu\n", GetLastError());
            abort();
        } else if (length >= sizeof(exe_name)) {
            fprintf(stderr, "Could not get executable name, buffer too small\n");
            abort();
        }
        status.executable_name = (struct TdoString) { .bytes=exe_name, .length=length };
        fprintf(stderr, "Executable name: '%s'\n", status.executable_name.bytes);

        status.iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (status.iocp == NULL) {
            fprintf(stderr, "could not create IOCP: %lu\n", GetLastError());
            result = TDO_ERROR_PIPE;
            goto error_setup;
        }

        status.job = CreateJobObject(NULL, NULL);
        if (status.job == NULL) {
            fprintf(stderr, "could not create job object: %lu\n", GetLastError());
            result = TDO_ERROR_OS;
            goto error_job_setup;
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (!SetInformationJobObject(status.job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
            result = TDO_ERROR_OS;
            goto error_job_settings;
        }

        QueryPerformanceFrequency(&status.clock_frequency);
    #else
        #error "Unknown platform"
    #endif
    
    for (size_t i = 0; i < args.processes; i++) {
        status.runs[i] = (struct TdoRun) {
            .test = NULL,
            .out = tdo_log_init(TDO_FILE_DESCRIPTOR_INVALID),
            .err = tdo_log_init(TDO_FILE_DESCRIPTOR_INVALID),
            .status = tdo_log_init(TDO_FILE_DESCRIPTOR_INVALID),
            .active = false,
        };
    }

    fprintf(output, "[");

    while (status.finished < tests.length) {
        while (status.running < args.processes && status.started < tests.length && !status.fork_failed && !status.log_setup_failed) {
            fflush(stdout);
            fflush(output);
            fflush(stderr); // normally unbuffered, but it could be buffered

            struct TdoTest *test = &((struct TdoTest*) tests.data)[status.started];
            struct TdoRun *run = tdo_run_new(args.processes, status.runs);
            if (run == NULL) {
                fprintf(stderr, "could not start new test run, all test runs are active\n");
                abort();
            }
            status.started += 1;

            tdo_run_start_new(&status, arena, args, output, test, run);
        }

        tdo_run_poll_event(&status, arena, args, output, tests);

        for (size_t i = 0; i < args.processes; i++) {
            struct TdoRun *run = &status.runs[i];
            if (run->active) {
                tdo_run_poll_exit(run, &status, arena, output);
            }
        }

        if (status.running == 0 && status.fork_failed) {
            struct TdoTest *ts = tests.data;
            for (size_t i = status.started; i < tests.length; i++) {
                if (status.finished > 0) fprintf(output, ",");
                tdo_run_report_error(ts[i], output, NULL, "could not fork process", -1.0);
                status.finished += 1;
            }
        } else if (status.running == 0 && status.log_setup_failed) {
            struct TdoTest *ts = tests.data;
            for (size_t i = status.started; i < tests.length; i++) {
                if (status.finished > 0) fprintf(output, ",");
                tdo_run_report_error(ts[i], output, NULL, "could not setup log redirection", -1.0);
                status.finished += 1;
            }
        }
    }

    fprintf(output, "\n]\n");
    result = TDO_ERROR_OK;

    #if defined(TDO_WINDOWS)
        error_job_settings:
        CloseHandle(status.job);
        error_job_setup:
        CloseHandle(status.iocp);
    #endif
    error_setup:
    tdo_arena_state_set(arena, state);
    return result;
}
