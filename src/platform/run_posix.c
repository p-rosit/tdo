#include "../error.h"
#include "../arguments.h"
#include "../test.h"
#include "../str.h"
#include "../run.h"
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

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
        if (runs[i].active) continue;

        return &runs[i];
    }
    return NULL;
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

enum TdoError tdo_log_drain(struct TdoLog *log, struct TdoArena *arena) {
    char buffer[1024];
    while (true) {
        errno = 0;
        ssize_t bytes_read = read(log->fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            enum TdoError err = tdo_log_append(log, arena, bytes_read, buffer);
            if (err != TDO_ERROR_OK) return err;
        } else if (bytes_read == 0) {
            break;
        } else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        } else {
            return TDO_ERROR_PIPE;
        }
    }

    return TDO_ERROR_OK;
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

void tdo_run_start_new(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoTest *test, struct TdoRun *run) {
    int p_out[2], p_err[2], p_status[2];
    if (pipe(p_out) || pipe(p_err) || pipe(p_status)) {
        // we should possibly close the pipes we were able to open
        // here, but we're just about to exit anyway, so who cares
        status->log_setup_failed = true;
        status->started -= 1;
        return;
    }

    errno = 0;
    FILE *status_file = fdopen(p_status[1], "wb");
    if (errno != 0 || status == NULL) {
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

            tdo_run_single(test, arena, status_file);

            // flush buffers
            fflush(stdout);
            fflush(stderr);
            fflush(status_file);
            _exit(0);
        case -1:
            status->fork_failed = true;
            status->started -= 1;
            return;
    }

    close(p_out[1]); close(p_err[1]); fclose(status_file);
    run->active = true;
    run->test = test;
    run->start_time = start_time;
    run->pid = pid;
    tdo_log_reset(&run->out, p_out[0]);
    tdo_log_reset(&run->err, p_err[0]);
    tdo_log_reset(&run->status, p_status[0]);

    status->running += 1;
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
            tdo_run_report_status(run, arena, output, return_status, duration, false);
        } else if (out_err == TDO_ERROR_MEMORY || err_err == TDO_ERROR_MEMORY || status_err == TDO_ERROR_MEMORY) {
            tdo_run_report_error(*run->test, output, NULL, "could not allocate space for output", duration);
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

void tdo_run_poll_event(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoArray tests) {
    size_t fd_count = tdo_run_assemble_active_fds(status->fds, status->fd_to_idx, args.processes, status->runs);

    errno = 0;
    if (poll(status->fds, fd_count, 100) > 0) {
        for (size_t i = 0; i < fd_count; i++) {
            if (status->fds[i].revents & POLLIN) {
                struct TdoRun *run = &status->runs[status->fd_to_idx[i]];
                if (!run->active) continue;

                enum TdoError err;
                if (status->fds[i].fd == run->out.fd) {
                    err = tdo_log_drain(&run->out, arena);
                } else if (status->fds[i].fd == run->err.fd) {
                    err = tdo_log_drain(&run->err, arena);
                } else {
                    err = tdo_log_drain(&run->status, arena);
                }

                if (err != TDO_ERROR_OK) {
                    TdoMonotoneTime end_time = tdo_time_get();
                    double duration = (
                        (double)(end_time.tv_sec - run->start_time.tv_sec)
                        + (double)(end_time.tv_nsec - run->start_time.tv_nsec) * 1e-9
                    );

                    if (status->finished > 0) fprintf(output, ",");
                    if (err == TDO_ERROR_MEMORY) {
                        tdo_run_report_error(*run->test, output, NULL, "could not allocate space for output", duration);
                    } else {
                        tdo_run_report_error(*run->test, output, NULL, "could not read output", duration);
                    }

                    kill(run->pid, SIGKILL);
                    run->active = false;
                    close(run->out.fd);
                    close(run->err.fd);
                    close(run->status.fd);
                    status->running -= 1;
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
    
    struct timespec end_time = tdo_time_get();

    for (size_t i = 0; i < args.processes; i++) {
        struct TdoRun *run = &status->runs[i];
        if (run->active) {
            tdo_run_poll_exit(run, status, arena, output);
            if (run->active) {
                double duration = (
                    (double)(end_time.tv_sec - run->start_time.tv_sec)
                    + (double)(end_time.tv_nsec - run->start_time.tv_nsec) * 1e-9
                );
                if (duration > args.time_limit) {
                    // timeout
                    if (status->finished > 0) fprintf(output, ",");
                    tdo_run_report_status(run, arena, output, 0, duration, true);

                    kill(run->pid, SIGKILL);
                    run->active = false;
                    close(run->out.fd);
                    close(run->err.fd);
                    close(run->status.fd);
                    status->running -= 1;
                    status->finished += 1;
                }
            }
        }
    }
}

enum TdoError tdo_run_status_init(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args) {
    enum TdoError result = TDO_ERROR_UNKNOWN;
    struct TdoArenaState state = tdo_arena_state_get(arena);

    *status = (struct TdoRunStatus) {
        .runs = NULL,
        .fds = NULL,
        .fd_to_idx = NULL,
        .started = 0,
        .finished = 0,
        .running = 0,
        .fork_failed = false,
        .log_setup_failed = false,
    };

    status->runs = tdo_arena_alloc(arena, sizeof(struct TdoRun), args.processes);
    if (status->runs == NULL) {
        result = TDO_ERROR_MEMORY;
        goto error_setup;
    }

    if (args.processes > SIZE_MAX / 3) return TDO_ERROR_MEMORY;
    status->fds = tdo_arena_alloc(arena, sizeof(struct pollfd), 3 * args.processes);
    if (status->fds == NULL) {
        result = TDO_ERROR_MEMORY;
        goto error_setup;
    }

    status->fd_to_idx = tdo_arena_alloc(arena, sizeof(size_t), 3 * args.processes);
    if (status->fd_to_idx == NULL) {
        result = TDO_ERROR_MEMORY;
        goto error_setup;
    }
    
    for (size_t i = 0; i < args.processes; i++) {
        status->runs[i] = (struct TdoRun) {
            .test = NULL,
            .out = tdo_log_init(TDO_FILE_DESCRIPTOR_INVALID),
            .err = tdo_log_init(TDO_FILE_DESCRIPTOR_INVALID),
            .status = tdo_log_init(TDO_FILE_DESCRIPTOR_INVALID),
            .active = false,
        };
    }

    result = TDO_ERROR_OK;

    error_setup:
    if (result != TDO_ERROR_OK) tdo_arena_state_set(arena, state);
    return result;
}

void tdo_run_status_deinit(struct TdoRunStatus status, struct TdoArguments args) {
    (void)status;
    (void)args;
}
