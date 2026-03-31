#include "error.h"
#include "arguments.c"
#include "test.c"

#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>

struct TdoRun {
    struct TdoTest *test;
    struct TdoLog out;
    struct TdoLog err;
    struct TdoLog status;
    pid_t pid;
    bool active;
};

struct TdoRun *tdo_run_new(size_t length, struct TdoRun *runs) {
    for (size_t i = 0; i < length; i++) {
        if (!runs[i].active) return &runs[i];
    }
    return NULL;
}

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

void tdo_json_escaped(FILE *file, struct TdoString string) {
    for (size_t i = 0; i < string.length; i++) {
        switch (string.bytes[i]) {
            case '\"': fputs("\\\"", file); break;
            case '\\': fputs("\\\\", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            default: fputc(string.bytes[i], file); break;
        }
    }
}

void tdo_log_dump(struct TdoLog log, FILE *file, char const *name) {
    fprintf(file, ",\n\t\t\"%s\": \"", name);
    tdo_json_escaped(file, log.data);
    fprintf(file, "\"");
}

void tdo_run_report_exit(struct TdoRun run, FILE *file, int status) {
    fprintf(file, "\n");
    fprintf(file, "\t{\n");

    fprintf(file, "\t\t\"file\": \"");
    tdo_json_escaped(file, run.test->symbol.file->name);
    fprintf(file, "\",\n");

    fprintf(file, "\t\t\"name\": \"");
    tdo_json_escaped(file, run.test->symbol.name);
    fprintf(file, "\",\n");

    fprintf(file, "\t\t\"status\": \"");
    if (WIFEXITED(status)) {
        fprintf(file, "exit");
    } else if (WIFSIGNALED(status)) {
        fprintf(file, "signal");
    } else if (WIFSTOPPED(status)) {
        fprintf(file, "stop");
    }
    fprintf(file, "\",\n");

    if (WIFEXITED(status)) {
        fprintf(file, "\t\t\"exit\": %d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        fprintf(file, "\t\t\"signal\": %d", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        fprintf(file, "\t\t\"stop\": %d", WSTOPSIG(status));
    }

    tdo_log_dump(run.out, file, "file");
    tdo_log_dump(run.err, file, "stderr");
    tdo_log_dump(run.status, file, "status");

    fprintf(file, "\n\t}");
}

void tdo_run_report_error(struct TdoTest test, FILE *file, char const *error) {
    fprintf(file, "\n");
    fprintf(file, "\t{\n");

    fprintf(file, "\t\t\"file\": \"");
    tdo_json_escaped(file, test.symbol.file->name);
    fprintf(file, "\",\n");

    fprintf(file, "\t\t\"name\": \"");
    tdo_json_escaped(file, test.symbol.name);
    fprintf(file, "\",\n");

    fprintf(file, "\t\t\"status\": \"error\",\n");
    fprintf(file, "\t\t\"error\": \"%s\"\n", error);

    fprintf(file, "\n\t}");
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
            fflush(output);
            fflush(stderr); // normally unbuffered, but it could be buffered

            struct TdoTest *test = &((struct TdoTest*) tests.data)[status.started];
            struct TdoRun *run = tdo_run_new(args.processes, status.runs);
            if (run == NULL) {
                fprintf(stderr, "could not start new test run, all test runs are active");
                abort();
            }
            status.started += 1;

            int p_out[2], p_err[2], p_status[2];
            if (pipe(p_out) || pipe(p_err) || pipe(p_status)) {
                // we should possibly close the pipes we were able to open
                // here, but we're just about to exit anyway, so who cares
                status.log_setup_failed = true;
                status.started -= 1;
                continue;
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

            pid_t pid = fork();
            switch (pid) {
                case 0:
                    // child

                    // close all pipe fds inherited from parent's other active runs
                    for (size_t i = 0; i < args.processes; i++) {
                        if (status.runs[i].active) {
                            close(status.runs[i].out.fd);
                            close(status.runs[i].err.fd);
                            close(status.runs[i].status.fd);
                        }
                    }

                    // close write end of input pipes
                    close(p_out[0]); close(p_err[0]); close(p_status[0]);

                    // replace stdout/stderr
                    dup2(p_out[1], STDOUT_FILENO);
                    dup2(p_err[1], STDERR_FILENO);

                    char status_buffer[64]; // must fit "b_18446744073709551615"

                    dlerror(); // clear old errors for good luck

                    // run before fixtures
                    struct TdoFixture *fixtures = test->fixtures.data;
                    for (size_t i = 0, index = 0; i < test->fixtures.length; i++) {
                        struct TdoFixture fixture = fixtures[i];
                        if (fixture.kind == TDO_FIXTURE_BEFORE) {
                            int length = snprintf(status_buffer, sizeof(status_buffer), "b_%zu\n", index++);
                            if (length < 0) {
                                write(p_status[1], "Could not format status string\n", 31);
                                abort();
                            } if (length >= sizeof(status_buffer)) {
                                write(p_status[1], "Could not format status string, result too long\n", 48);
                                abort();
                            }
                            write(p_status[1], status_buffer, length);

                            void (*fix)(void) = (void (*)(void))dlsym(fixture.symbol.file->dynamic_handle, fixture.symbol.name.bytes);
                            char const *err = dlerror();
                            if (err != NULL) {
                                write(p_status[1], "eCould not load before fixture: ", 32);
                                write(p_status[1], err, strlen(err));
                                write(p_status[1], "\n", 1);
                                abort();
                            } else if (fix == NULL) {
                                write(p_status[1], "eSymbol is null\n", 16);
                                abort();
                            }
                            fix();
                        }
                    }

                    // do the test
                    write(p_status[1], "test\n", 5);
                    void (*t)(void) = (void (*)(void))dlsym(test->symbol.file->dynamic_handle, test->symbol.name.bytes);
                    char const *err = dlerror();
                    if (err != NULL) {
                        write(p_status[1], "eCould not load test: ", 22);
                        write(p_status[1], err, strlen(err));
                        write(p_status[1], "\n", 1);
                        abort();
                    } else if (t == NULL) {
                        write(p_status[1], "eSymbol is null\n", 16);
                        abort();
                    }
                    t();

                    // run after fixtures
                    for (size_t i = 0, index = 0; i < test->fixtures.length; i++) {
                        struct TdoFixture fixture = fixtures[i];
                        if (fixture.kind == TDO_FIXTURE_AFTER) {
                            int length = snprintf(status_buffer, sizeof(status_buffer), "a_%zu\n", index++);
                            if (length < 0) {
                                write(p_status[1], "Could not format status string\n", 31);
                                abort();
                            } if (length >= sizeof(status_buffer)) {
                                write(p_status[1], "Could not format status string, result too long\n", 48);
                                abort();
                            }
                            write(p_status[1], status_buffer, length);

                            void (*fix)(void) = (void (*)(void))dlsym(fixture.symbol.file->dynamic_handle, fixture.symbol.name.bytes);
                            char const *err = dlerror();
                            if (err != NULL) {
                                write(p_status[1], "eCould not load after fixture: ", 31);
                                write(p_status[1], err, strlen(err));
                                write(p_status[1], "\n", 1);
                                abort();
                            } else if (fix == NULL) {
                                write(p_status[1], "eSymbol is null\n", 16);
                                abort();
                            }
                            fix();
                        }
                    }

                    write(p_status[1], "finished\n", 9);

                    // flush buffers
                    fflush(stdout);
                    fflush(stderr);
                    _exit(0);
                case -1:
                    status.fork_failed = true;
                    status.started -= 1;
                    continue;
            }

            close(p_out[1]); close(p_err[1]); close(p_status[1]);
            run->active = true;
            run->test = test;
            run->pid = pid;
            tdo_log_reset(&run->out, p_out[0]);
            tdo_log_reset(&run->err, p_err[0]);
            tdo_log_reset(&run->status, p_status[0]);

            status.running += 1;
        }

        size_t fd_count = tdo_run_assemble_active_fds(status.fds, status.fd_to_idx, args.processes, status.runs);

        errno = 0;
        if (poll(status.fds, fd_count, 100) > 0) {
            for (size_t i = 0; i < fd_count; i++) {
                if (status.fds[i].revents & POLLIN) {
                    struct TdoRun *run = &status.runs[status.fd_to_idx[i]];

                    enum TdoError err;
                    if (status.fds[i].fd == run->out.fd) {
                        err = tdo_log_drain(&run->out, arena);
                    } else if (status.fds[i].fd == run->err.fd) {
                        err = tdo_log_drain(&run->err, arena);
                    } else {
                        err = tdo_log_drain(&run->status, arena);
                    }

                    if (err != TDO_ERROR_OK) {
                        run->active = false;
                        status.running -= 1;
                        if (status.finished > 0) fprintf(output, ",");
                        tdo_run_report_error(*run->test, output, "could not read output");
                        status.finished += 1;
                    }
                }
            }
        } else if (errno != 0 && errno != EINTR && errno != EAGAIN) {
            perror("poll failed");

            for (size_t i = 0; i < args.processes; i++) {
                struct TdoRun *run = &status.runs[i];
                if (run->active) {
                    tdo_run_report_error(*run->test, output, "could not poll pipes");
                    status.finished += 1;
                }
            }

            struct TdoTest *ts = tests.data;
            for (size_t i = status.started; i < tests.length; i++) {
                if (status.finished > 0) fprintf(output, ",");
                tdo_run_report_error(ts[i], output, "could not poll pipes");
                status.finished += 1;
            }
            break;
        }

        for (size_t i = 0; i < args.processes; i++) {
            struct TdoRun *run = &status.runs[i];
            if (run->active) {
                int return_status;
                if (waitpid(run->pid, &return_status, WNOHANG) > 0) {
                    enum TdoError out_err = tdo_log_drain(&run->out, arena);
                    enum TdoError err_err = tdo_log_drain(&run->err, arena);
                    enum TdoError status_err = tdo_log_drain(&run->status, arena);

                    if (status.finished > 0) fprintf(output, ",");
                    if (out_err == TDO_ERROR_OK && err_err == TDO_ERROR_OK && status_err == TDO_ERROR_OK)  {
                        tdo_run_report_exit(*run, output, return_status);
                    } else {
                        tdo_run_report_error(*run->test, output, "could not read output");
                    }

                    run->active = false;
                    close(run->out.fd);
                    close(run->err.fd);
                    close(run->status.fd);
                    status.running -= 1;
                    status.finished += 1;
                }
            }
        }

        if (status.running == 0 && status.fork_failed) {
            struct TdoTest *ts = tests.data;
            for (size_t i = status.started; i < tests.length; i++) {
                if (status.finished > 0) fprintf(output, ",");
                tdo_run_report_error(ts[i], output, "could not fork process");
                status.finished += 1;
            }
        } else if (status.running == 0 && status.log_setup_failed) {
            struct TdoTest *ts = tests.data;
            for (size_t i = status.started; i < tests.length; i++) {
                if (status.finished > 0) fprintf(output, ",");
                tdo_run_report_error(ts[i], output, "could not setup log redirection");
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
