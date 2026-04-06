#include "error.h"
#include "arguments.c"
#include "test.c"

struct TdoRun {
    struct TdoTest *test;
    struct TdoLog out;
    struct TdoLog err;
    struct TdoLog status;
    TdoMonotoneTime start_time;
    pid_t pid;
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

void tdo_run_report_exit(struct TdoRun run, FILE *file, char const *step, int status, double duration) {
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
    if (WIFEXITED(status)) {
        if (step[0] == 'f') {
            fprintf(file, "complete");
        } else {
            fprintf(file, "exit");
        }
    } else if (WIFSIGNALED(status)) {
        fprintf(file, "signal");
    } else if (WIFSTOPPED(status)) {
        fprintf(file, "stop");
    }
    fprintf(file, "\"");

    if (WIFEXITED(status) && step[0] != 'f') {
        fprintf(file, ",\n\t\t\"exit\": %d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        fprintf(file, ",\n\t\t\"signal\": %d", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        fprintf(file, ",\n\t\t\"stop\": %d", WSTOPSIG(status));
    }

    if (step[0] != 'f') {
        fprintf(file, ",\n\t\t\"step\": \"%s\"", step);
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
    fprintf(file, "\t\t\"error\": \"%s\",\n", error);

    fprintf(file, "\t\t\"step\": ");
    if (step != NULL) {
        fprintf(file, "\"%s\"\n", step);
    } else {
        fprintf(file, "null\n");
    }

    fprintf(file, "\t}");
}

struct TdoRunStatus {
    struct TdoRun *runs;
    struct pollfd *fds;
    size_t *fd_to_idx;
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

void tdo_run_single(struct TdoTest *test, struct TdoArena *arena, TdoFileDescriptor status_fd) {
    char status_buffer[64]; // must fit "b_18446744073709551615"

    // run before fixtures
    struct TdoFixture *fixtures = test->fixtures.data;
    for (size_t i = 0, index = 0; i < test->fixtures.length; i++) {
        struct TdoFixture fixture = fixtures[i];
        if (fixture.kind == TDO_FIXTURE_BEFORE) {
            int length = snprintf(status_buffer, sizeof(status_buffer), "b_%zu\n", index++);
            if (length < 0) {
                tdo_write_fd(status_fd, 32, "eCould not format status string\n");
                abort();
            } if (length >= sizeof(status_buffer)) {
                tdo_write_fd(status_fd, 49, "eCould not format status string, result too long\n");
                abort();
            }
            tdo_write_fd(status_fd, length, status_buffer);

            if (fixture.symbol.file->library == NULL) {
                tdo_write_fd(status_fd, 25, "eCould not load library: ");
                tdo_write_fd(status_fd, fixture.symbol.file->name.length, fixture.symbol.file->name.bytes);
                tdo_write_fd(status_fd, 1, "\n");
                abort();
            }

            struct TdoSymbolLoadResult fixture_result = tdo_dynamic_symbol_load(fixture.symbol.file->library, fixture.symbol.name.bytes, arena);
            if (fixture_result.err != NULL) {
                tdo_write_fd(status_fd, 32, "eCould not load before fixture: ");
                tdo_write_fd(status_fd, strlen(fixture_result.err), fixture_result.err);
                tdo_write_fd(status_fd, 1, "\n");
                abort();
            } else if (fixture_result.symbol == NULL) {
                tdo_write_fd(status_fd, 16, "eSymbol is null\n");
                abort();
            }
            void (*fix)(void) = fixture_result.symbol;
            fix();
        }
    }

    // do the test
    tdo_write_fd(status_fd, 5, "test\n");

    if (test->symbol.file->library == NULL) {
        tdo_write_fd(status_fd, 25, "eCould not load library: ");
        tdo_write_fd(status_fd, test->symbol.file->name.length, test->symbol.file->name.bytes);
        tdo_write_fd(status_fd, 1, "\n");
        abort();
    }

    struct TdoSymbolLoadResult symbol_result = tdo_dynamic_symbol_load(test->symbol.file->library, test->symbol.name.bytes, arena);
    if (symbol_result.err != NULL) {
        tdo_write_fd(status_fd, 22, "eCould not load test: ");
        tdo_write_fd(status_fd, strlen(symbol_result.err), symbol_result.err);
        tdo_write_fd(status_fd, 1, "\n");
        abort();
    } else if (symbol_result.symbol == NULL) {
        tdo_write_fd(status_fd, 16, "eSymbol is null\n");
        abort();
    }
    void (*t)(void) = symbol_result.symbol;
    t();

    // run after fixtures
    for (size_t i = 0, index = 0; i < test->fixtures.length; i++) {
        struct TdoFixture fixture = fixtures[i];
        if (fixture.kind == TDO_FIXTURE_AFTER) {
            int length = snprintf(status_buffer, sizeof(status_buffer), "a_%zu\n", index++);
            if (length < 0) {
                tdo_write_fd(status_fd, 32, "eCould not format status string\n");
                abort();
            } if (length >= sizeof(status_buffer)) {
                tdo_write_fd(status_fd, 49, "eCould not format status string, result too long\n");
                abort();
            }
            tdo_write_fd(status_fd, length, status_buffer);

            if (fixture.symbol.file->library == NULL) {
                tdo_write_fd(status_fd, 25, "eCould not load library: ");
                tdo_write_fd(status_fd, fixture.symbol.file->name.length, fixture.symbol.file->name.bytes);
                tdo_write_fd(status_fd, 1, "\n");
                abort();
            }

            struct TdoSymbolLoadResult fixture_result = tdo_dynamic_symbol_load(fixture.symbol.file->library, fixture.symbol.name.bytes, arena);
            if (fixture_result.err != NULL) {
                tdo_write_fd(status_fd, 31, "eCould not load after fixture: ");
                tdo_write_fd(status_fd, strlen(fixture_result.err), fixture_result.err);
                tdo_write_fd(status_fd, 1, "\n");
                abort();
            } else if (fixture_result.symbol == NULL) {
                tdo_write_fd(status_fd, 16, "eSymbol is null\n");
                abort();
            }
            void (*fix)(void) = fixture_result.symbol;
            fix();
        }
    }

    tdo_write_fd(status_fd, 9, "finished\n");
}

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
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

    void tdo_run_start_new(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoArray tests) {
        fflush(stdout);
        fflush(output);
        fflush(stderr); // normally unbuffered, but it could be buffered

        struct TdoTest *test = &((struct TdoTest*) tests.data)[status->started];
        struct TdoRun *run = tdo_run_new(args.processes, status->runs);
        if (run == NULL) {
            fprintf(stderr, "could not start new test run, all test runs are active");
            abort();
        }
        status->started += 1;

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
#elif defined(_WIN32)
    void tdo_run_start_new(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoArray tests);

    void tdo_run_poll_event(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoArray tests);

    void tdo_run_poll_exit(struct TdoRun *run, struct TdoRunStatus *status, struct TdoArena *arena, FILE *output);
#else
    #error "Unknown platform"
#endif

enum TdoError tdo_run_all(struct TdoArguments args, FILE *output, struct TdoArena *arena, struct TdoArray tests) {
    enum TdoError result = TDO_ERROR_UNKNOWN;
    struct TdoArenaState state = tdo_arena_state_get(arena);
    fprintf(stderr, "Running %zu tests\n", tests.length);

    struct TdoRunStatus status = {
        .runs = NULL,
        .fds = NULL,
        .fd_to_idx = NULL,
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
    
    for (size_t i = 0; i < args.processes; i++) {
        status.runs[i] = (struct TdoRun) {
            .test = NULL,
            .out = tdo_log_init(0),
            .err = tdo_log_init(0),
            .status = tdo_log_init(0),
            .active = false,
        };
    }

    fprintf(output, "[");

    while (status.finished < tests.length) {
        while (status.running < args.processes && status.started < tests.length && !status.fork_failed && !status.log_setup_failed) {
            tdo_run_start_new(&status, arena, args, output, tests);
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

    error_setup:
    tdo_arena_state_set(arena, state);
    return result;
}
