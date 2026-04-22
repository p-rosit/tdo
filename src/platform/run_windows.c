#include "../error.h"
#include "../arguments.h"
#include "../test.c"
#include <windows.h>

struct TdoOverlap {
    OVERLAPPED overlapped;
    enum {
        TDO_PIPE_IDLE,
        TDO_PIPE_WAITING,
        TDO_PIPE_CONNECTED,
        TDO_PIPE_CANCELLING,
    } status;
    enum { TDO_LOG_ERR, TDO_LOG_OUT, TDO_LOG_STATUS } kind;
    char buffer[1024];
};

struct TdoRun {
    struct TdoTest *test;
    struct TdoLog out;
    struct TdoLog err;
    struct TdoLog status;
    TdoMonotoneTime start_time;
    DWORD pid;
    HANDLE process_handle;
    DWORD exit_code;
    struct TdoString out_name;
    struct TdoString err_name;
    struct TdoString status_name;
    bool active;
    struct TdoOverlap out_ov;
    struct TdoOverlap err_ov;
    struct TdoOverlap status_ov;
};

int tdo_run_pipes_pending(struct TdoRun *run) {
    return (
        (run->out_ov.status == TDO_PIPE_CONNECTED)
        + (run->err_ov.status == TDO_PIPE_CONNECTED)
        + (run->status_ov.status == TDO_PIPE_CONNECTED)
    );
}

int tdo_run_pipes_cancelling(struct TdoRun *run) {
    return (
        (run->out_ov.status == TDO_PIPE_CANCELLING)
        + (run->err_ov.status == TDO_PIPE_CANCELLING)
        + (run->status_ov.status == TDO_PIPE_CANCELLING)
    );
}

struct TdoRun *tdo_run_new(size_t length, struct TdoRun *runs) {
    for (size_t i = 0; i < length; i++) {
        if (runs[i].active) continue;
        if (tdo_run_pipes_cancelling(&runs[i]) > 0) continue;

        return &runs[i];
    }
    return NULL;
}

struct TdoRunStatus {
    struct TdoRun *runs;
    HANDLE job;
    HANDLE iocp;
    LARGE_INTEGER clock_frequency;
    DWORD pid;
    struct TdoString executable_name;
    size_t started;
    size_t finished;
    size_t running;
    bool fork_failed;
    bool log_setup_failed;
};

enum TdoError tdo_pipe_connect(struct TdoRun *run, struct TdoOverlap *overlap) {
    struct TdoLog *log;
    switch (overlap->kind) {
        case TDO_LOG_OUT: log = &run->out; break;
        case TDO_LOG_ERR: log = &run->err; break;
        case TDO_LOG_STATUS: log = &run->status; break;
        default: fprintf(stderr, "Invalid enum value: %d\n", overlap->kind); fflush(NULL); abort();
    }

    ZeroMemory(&overlap->overlapped, sizeof(overlap->overlapped));
    overlap->status = TDO_PIPE_WAITING;

    BOOL connected = ConnectNamedPipe(log->fd, &overlap->overlapped);
    DWORD code = GetLastError();
    if (connected || code == ERROR_PIPE_CONNECTED || code == ERROR_BROKEN_PIPE || code == ERROR_NO_DATA) {
        fprintf(stderr, "Unexpected connection while connecting pipe, got windows error code: %lu\n", code);
        fflush(NULL);
        abort();
    } else if (code == ERROR_IO_PENDING) {
        // waiting for IOCP packet
    } else {
        fprintf(stderr, "Could not connect pipe, got windows error code: %lu\n", code);
        overlap->status = TDO_PIPE_IDLE;
        return TDO_ERROR_OS;
    }

    return TDO_ERROR_OK;
}

void tdo_run_start_new(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoTest *test, struct TdoRun *run) {
    (void)args; // unused
    (void)output; // unused
    struct TdoArenaState state = tdo_arena_state_get(arena);

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
                default: fprintf(stderr, "Invalid fixture kind: %d\n", fixture.kind); fflush(NULL); abort();
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
        if (!(could_build = tdo_string_append(&command, arena, run->status_name.length, run->status_name.bytes))) goto error_build_command;
        
        error_build_command:
        if (!could_build) {
            status->fork_failed = true;
            fprintf(stderr, "Could not allocate command to start test: %s::%s\n", test->symbol.file->name.bytes, test->symbol.name.bytes);
            goto error_setup;
        }
    }

    run->test = test;
    run->pid = 0;
    run->process_handle = NULL;
    run->exit_code = 0;
    tdo_log_reset(&run->out, run->out.fd);
    tdo_log_reset(&run->err, run->err.fd);
    tdo_log_reset(&run->status, run->status.fd);
    run->out_ov.status = TDO_PIPE_WAITING;
    run->err_ov.status = TDO_PIPE_WAITING;
    run->status_ov.status = TDO_PIPE_WAITING;

    // Start opening connection
    enum TdoError err_read = tdo_pipe_connect(run, &run->out_ov);
    if (err_read != TDO_ERROR_OK) {
        fprintf(stderr, "Could not start reading from child\n");
        status->log_setup_failed = true;
        goto error_setup;
    }

    err_read = tdo_pipe_connect(run, &run->err_ov);
    if (err_read != TDO_ERROR_OK) {
        fprintf(stderr, "Could not start reading from child\n");
        status->log_setup_failed = true;
        goto error_err_pipe;
    }

    err_read = tdo_pipe_connect(run, &run->status_ov);
    if (err_read != TDO_ERROR_OK) {
        fprintf(stderr, "Could not start reading from child\n");
        status->log_setup_failed = true;
        goto error_status_pipe;
    }
    
    SECURITY_ATTRIBUTES sa;
    {
        ZeroMemory(&sa, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
    }

    HANDLE h_out_client = CreateFile(
        run->out_name.bytes,
        GENERIC_WRITE,
        0,
        &sa, // Must be inheritable
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, // <--- Synchronous!
        NULL
    );
    if (h_out_client == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Could not open stdout pipe handle: %lu\n", GetLastError());
        status->log_setup_failed = true;
        goto error_out_client;
    }
    
    HANDLE h_err_client = CreateFile(
        run->err_name.bytes,
        GENERIC_WRITE,
        0,
        &sa, // Must be inheritable
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, // <--- Synchronous!
        NULL
    );
    if (h_err_client == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Could not open stderr pipe handle: %lu\n", GetLastError());
        status->log_setup_failed = true;
        goto error_err_client;
    }

    STARTUPINFO si;
    {
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = INVALID_HANDLE_VALUE;
        si.hStdOutput = h_out_client;
        si.hStdError = h_err_client;
    }

    PROCESS_INFORMATION pi;
    TdoMonotoneTime start_time = tdo_time_get();
    if (!CreateProcess(NULL, command.bytes, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "Could not start process...\n");
        status->fork_failed = true;
        goto error_process_start;
    }

    if (!AssignProcessToJobObject(status->job, pi.hProcess)) {
        fprintf(stderr, "Could not assign process to job\n");
        status->fork_failed = true;
        TerminateProcess(pi.hProcess, 1); // child was not tethered to parent
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        goto error_process_start;
    }

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    run->active = true;
    run->start_time = start_time;
    run->pid = pi.dwProcessId;
    run->process_handle = pi.hProcess;
    status->running += 1;

    error_process_start:
    CloseHandle(h_err_client);
    error_err_client:
    CloseHandle(h_out_client);
    error_out_client:
    if (run->process_handle == NULL && run->status_ov.status == TDO_PIPE_WAITING) {
        CancelIoEx(run->status.fd, (LPOVERLAPPED) &run->status_ov);
        run->status_ov.status = TDO_PIPE_CANCELLING;
    }
    error_status_pipe:
    if (run->process_handle == NULL && run->err_ov.status == TDO_PIPE_WAITING) {
        CancelIoEx(run->err.fd, (LPOVERLAPPED) &run->err_ov);
        run->err_ov.status = TDO_PIPE_CANCELLING;
    }
    error_err_pipe:
    if (run->process_handle == NULL && run->out_ov.status == TDO_PIPE_WAITING) {
        CancelIoEx(run->out.fd, (LPOVERLAPPED) &run->out_ov);
        run->out_ov.status = TDO_PIPE_CANCELLING;
    }
    error_setup:
    if (run->process_handle == NULL) status->started -= 1;
    tdo_arena_state_set(arena, state);
    return;
}

void tdo_run_maybe_report_exit(struct TdoArena *arena, struct TdoRun *run, struct TdoRunStatus *status, FILE *output) {
    if (run->process_handle != NULL || tdo_run_pipes_pending(run) > 0) return;

    LARGE_INTEGER end_time = tdo_time_get();
    double duration = (double)(end_time.QuadPart - run->start_time.QuadPart) / status->clock_frequency.QuadPart;

    if (status->finished > 0) fprintf(output, ",");
    tdo_run_report_status(*run, arena, output, run->exit_code, duration);

    run->active = false;
    status->running -= 1;
    status->finished += 1;
}

void tdo_run_handle_exit(struct TdoArena *arena, struct TdoRun *run, struct TdoRunStatus *status, FILE *output, DWORD pid, DWORD msg) {
    if (msg != JOB_OBJECT_MSG_EXIT_PROCESS) return;

    if (run == NULL) {
        fprintf(stderr, "Process with unknown PID exited\n");
        fflush(NULL);
        abort();
    }

    DWORD return_status;
    if (!GetExitCodeProcess(run->process_handle, &return_status)) {
        fprintf(stderr, "Get exit code failed somehow... TODO: graceful exit\n");
        fflush(NULL);
        abort();
    }

    CloseHandle(run->process_handle);
    run->process_handle = NULL;
    run->exit_code = return_status;
    tdo_run_maybe_report_exit(arena, run, status, output);
}

void tdo_run_handle_pipe_disconnect(struct TdoArena *arena, struct TdoRun *run, struct TdoLog *log, struct TdoOverlap *overlap, struct TdoRunStatus *status, FILE *output) {
    overlap->status = TDO_PIPE_IDLE;
    DisconnectNamedPipe(log->fd);
    tdo_run_maybe_report_exit(arena, run, status, output);
}

void tdo_run_poll_event(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args, FILE *output, struct TdoArray tests) {
    (void)tests; // unused
    DWORD bytes_transferred;
    void *completion_key;
    LPOVERLAPPED overlapped;

    if (GetQueuedCompletionStatus(status->iocp, &bytes_transferred, (PULONG_PTR) &completion_key, &overlapped, 100)) {
        if (completion_key == NULL) { // process exited
            DWORD pid = (DWORD) overlapped;

            struct TdoRun *run = NULL;
            for (size_t i = 0; i < args.processes; i++) {
                struct TdoRun *r = &status->runs[i];
                if (r->active && r->pid == pid) {
                    run = r;
                    break;
                }
            }
            tdo_run_handle_exit(arena, run, status, output, pid, bytes_transferred);
            return;
        }

        // read file
        struct TdoRun *run = completion_key;
        struct TdoOverlap *ov = (struct TdoOverlap*) overlapped;
        struct TdoLog *log;
        switch (ov->kind) {
            case TDO_LOG_ERR: log = &run->err; break;
            case TDO_LOG_OUT: log = &run->out; break;
            case TDO_LOG_STATUS: log = &run->status; break;
            default: fprintf(stderr, "Invalid log type\n"); fflush(NULL); abort();
        }

        switch (ov->status) {
            case TDO_PIPE_CANCELLING:
                tdo_run_handle_pipe_disconnect(arena, run, log, ov, status, output);
                break;
            case TDO_PIPE_WAITING:
                ov->status = TDO_PIPE_CONNECTED;
                if (!ReadFile(log->fd, ov->buffer, sizeof(ov->buffer), NULL, &ov->overlapped)) {
                    DWORD code = GetLastError();
                    if (code == ERROR_IO_PENDING) {
                        // next read started
                    } else if (code == ERROR_BROKEN_PIPE || code == ERROR_PIPE_NOT_CONNECTED) {
                        tdo_run_handle_pipe_disconnect(arena, run, log, ov, status, output);
                    } else {
                        fprintf(stderr, "async ReadFile Failed: %lu\n", code);
                        fflush(NULL);
                        abort();
                    }
                }
                break;
            case TDO_PIPE_CONNECTED:
                if (bytes_transferred > 0) {
                    if (!tdo_string_append(&log->data, arena, bytes_transferred, ov->buffer)) {
                        run->active = false;
                        status->running -= 1;

                        LARGE_INTEGER end_time = tdo_time_get();
                        double duration = (double)(end_time.QuadPart - run->start_time.QuadPart) / status->clock_frequency.QuadPart;

                        if (status->finished > 0) fprintf(output, ",");
                        tdo_run_report_error(*run->test, output, NULL, "could not read output, ran out of memory", duration);
                        status->finished += 1;

                        TerminateProcess(run->process_handle, 1); // test produced more logs than we can read, why let it continue?
                        CloseHandle(run->process_handle);
                        run->active = false;
                        run->process_handle = NULL;

                        if (log != &run->out) {
                            CancelIoEx(run->out.fd, (LPOVERLAPPED) &run->out_ov);
                            run->out_ov.status = TDO_PIPE_CANCELLING;
                        } else {
                            run->out_ov.status = TDO_PIPE_IDLE;
                        }
                        if (log != &run->err) {
                            CancelIoEx(run->err.fd, (LPOVERLAPPED) &run->err_ov);
                            run->err_ov.status = TDO_PIPE_CANCELLING;
                        } else {
                            run->err_ov.status = TDO_PIPE_IDLE;
                        }
                        if (log != &run->status) {
                            CancelIoEx(run->status.fd, (LPOVERLAPPED) &run->status_ov);
                            run->status_ov.status = TDO_PIPE_CANCELLING;
                        } else {
                            run->status_ov.status = TDO_PIPE_IDLE;
                        }
                        return;
                    }

                    if (!ReadFile(log->fd, ov->buffer, sizeof(ov->buffer), NULL, &ov->overlapped)) {
                        DWORD code = GetLastError();
                        if (code == ERROR_IO_PENDING) {
                            // next read started
                        } else if (code == ERROR_BROKEN_PIPE || code == ERROR_PIPE_NOT_CONNECTED) {
                            tdo_run_handle_pipe_disconnect(arena, run, log, ov, status, output);
                        } else {
                            fprintf(stderr, "async ReadFile Failed: %lu\n", code);
                            fflush(NULL);
                            abort();
                        }
                    }
                } else {
                    // no bytes transferred, pipe closed
                    tdo_run_handle_pipe_disconnect(arena, run, log, ov, status, output);
                }
                break;
            case TDO_PIPE_IDLE: fprintf(stderr, "Idle pipe received completion packet\n"); fflush(NULL); abort();
            default: fprintf(stderr, "Invalid pipe state: %d\n", ov->status); fflush(NULL); abort();
        }
        return;
    }

    DWORD code = GetLastError();
    if (overlapped != NULL) {
        if (completion_key == NULL) {
            fprintf(stderr, "Dequeued failed completion packet related to process? msg=%lu, code=%lu\n", bytes_transferred, code);
            fflush(NULL);
            abort();
        }

        struct TdoOverlap *ov = (struct TdoOverlap*) overlapped;
        struct TdoRun *run = completion_key;
        struct TdoLog *log;
        switch (ov->kind) {
            case TDO_LOG_ERR: log = &run->err; break;
            case TDO_LOG_OUT: log = &run->out; break;
            case TDO_LOG_STATUS: log = &run->status; break;
            default: fprintf(stderr, "Invalid log type\n"); fflush(NULL); abort();
        }

        if (code == ERROR_BROKEN_PIPE || code == ERROR_OPERATION_ABORTED) {
            tdo_run_handle_pipe_disconnect(arena, run, log, ov, status, output);
        } else {
            fprintf(stderr, "Read from pipe failed: %lu\n", GetLastError());
            fflush(NULL);
            abort();
        }
    } else if (code == WAIT_TIMEOUT) {
        // timed out
    } else {
        fprintf(stderr, "Could not dequeue completion packet: %lu\n", GetLastError());
        fflush(NULL);
        abort();
    }
}

// longest pipe name: \\.\pipe\tdo_pipe_AAAAAA_18446744073709551615_4294967296_18446744073709551615
#define TDO_MAX_PIPE_NAME_LENGTH (78)

enum TdoError tdo_run_unique_pipe_name(struct TdoString *name, struct TdoArena *arena, char const *prefix, size_t index, DWORD pid, ULONGLONG ticks) {
    char buffer[TDO_MAX_PIPE_NAME_LENGTH];

    errno = 0;
    int total_written = snprintf(buffer, sizeof(buffer) - 1, "\\\\.\\pipe\\tdo_pipe_%s_%zu_%lu_%llu", prefix, index, pid, ticks);
    if (total_written >= sizeof(buffer) - 1) return TDO_ERROR_MEMORY; // buffer too small
    if (total_written < 0) {
        perror("Could not write unique name");
        return TDO_ERROR_UNKNOWN;
    }

    *name = tdo_string_init();
    if (!tdo_string_append(name, arena, total_written, buffer)) return TDO_ERROR_MEMORY;
    return TDO_ERROR_OK;
}

enum TdoError tdo_run_status_init(struct TdoRunStatus *status, struct TdoArena *arena, struct TdoArguments args) {
    enum TdoError result = TDO_ERROR_UNKNOWN;
    struct TdoArenaState state = tdo_arena_state_get(arena);

    *status = (struct TdoRunStatus) {
        .runs = NULL,
        .job = NULL,
        .iocp = NULL,
        .clock_frequency = { .QuadPart = 1 },
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

    char exe_name[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, (LPSTR) &exe_name, sizeof(exe_name));
    if (length == 0) {
        fprintf(stderr, "Could not get executable name: %lu\n", GetLastError());
        fflush(NULL);
        abort();
    } else if (length >= sizeof(exe_name)) {
        fprintf(stderr, "Could not get executable name, buffer too small\n");
        fflush(NULL);
        abort();
    }
    status->executable_name = tdo_string_init();
    if (!tdo_string_append(&status->executable_name, arena, length, exe_name)) {
        result = TDO_ERROR_MEMORY;
        goto error_setup;
    }

    status->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (status->iocp == NULL) {
        fprintf(stderr, "could not create IOCP: %lu\n", GetLastError());
        result = TDO_ERROR_PIPE;
        goto error_setup;
    }

    status->job = CreateJobObject(NULL, NULL);
    if (status->job == NULL) {
        fprintf(stderr, "could not create job object: %lu\n", GetLastError());
        result = TDO_ERROR_OS;
        goto error_job_setup;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(status->job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
        result = TDO_ERROR_OS;
        goto error_job_settings;
    }

    JOBOBJECT_ASSOCIATE_COMPLETION_PORT job_port;
    job_port.CompletionKey = NULL; // Unique ID for this job
    job_port.CompletionPort = status->iocp;
    if (!SetInformationJobObject(status->job, JobObjectAssociateCompletionPortInformation, &job_port, sizeof(job_port))) {
        fprintf(stderr, "SetInformationJobObject failed (%lu)\n", GetLastError());
        result = TDO_ERROR_OS;
        goto error_job_settings;
    }

    QueryPerformanceFrequency(&status->clock_frequency);

    for (size_t i = 0; i < args.processes; i++) {
        struct TdoRun *run = &status->runs[i];
        run->out.fd = INVALID_HANDLE_VALUE;
        run->err.fd = INVALID_HANDLE_VALUE;
        run->status.fd = INVALID_HANDLE_VALUE;
    }
    
    DWORD pid = GetCurrentProcessId();
    ULONGLONG ticks = GetTickCount64();
    SECURITY_ATTRIBUTES sa;
    {
        ZeroMemory(&sa, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = FALSE;
        sa.lpSecurityDescriptor = NULL;
    }

    for (size_t i = 0; i < args.processes; i++) {
        // open named pipes: stdout/stderr/status
        struct TdoString out_name;
        if ((result = tdo_run_unique_pipe_name(&out_name, arena, "out", i, pid, ticks)) != TDO_ERROR_OK) {
            fprintf(stderr, "Failed to format out pipe name\n");
            goto error_named_pipe_setup;
        }

        struct TdoString err_name;
        if ((result = tdo_run_unique_pipe_name(&err_name, arena, "err", i, pid, ticks)) != TDO_ERROR_OK) {
            fprintf(stderr, "Failed to format err pipe name\n");
            goto error_named_pipe_setup;
        }

        struct TdoString status_name;
        if ((result = tdo_run_unique_pipe_name(&status_name, arena, "status", i, pid, ticks)) != TDO_ERROR_OK) {
            fprintf(stderr, "Failed to format status pipe name\n");
            goto error_named_pipe_setup;
        }

        HANDLE h_out = CreateNamedPipe(
            out_name.bytes,
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_WAIT,            // Byte mode, overlapped
            1,                                     // Max instances
            1024,                                  // Out buffer
            1024,                                  // In buffer
            0,                                     // Default timeout
            &sa                                    // Cannot be inherited
        );
        if (GetLastError()) {
            fprintf(stderr, "Could not open out pipe: %lu\n", GetLastError());
            result = TDO_ERROR_OS;
            goto error_named_pipe_setup;
        }

        HANDLE h_err = CreateNamedPipe(
            err_name.bytes,
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_WAIT,            // Byte mode, overlapped
            1,                                     // Max instances
            1024,                                  // Out buffer
            1024,                                  // In buffer
            0,                                     // Default timeout
            &sa                                    // Cannot be inherited
        );
        if (GetLastError()) {
            fprintf(stderr, "Could not open err pipe: %lu\n", GetLastError());
            result = TDO_ERROR_OS;
            goto error_named_pipe_setup;
        }

        HANDLE h_status = CreateNamedPipe(
            status_name.bytes,
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_WAIT,            // Byte mode, overlapped
            1,                                     // Max instances
            1024,                                  // Out buffer
            1024,                                  // In buffer
            0,                                     // Default timeout
            &sa                                    // Cannot be inherited
        );
        if (GetLastError()) {
            fprintf(stderr, "Could not open status pipe: %lu\n", GetLastError());
            result = TDO_ERROR_OS;
            goto error_named_pipe_setup;
        }

        status->runs[i] = (struct TdoRun) {
            .test = NULL,
            .out = tdo_log_init(h_out),
            .err = tdo_log_init(h_err),
            .status = tdo_log_init(h_status),
            .out_name = out_name,
            .err_name = err_name,
            .status_name = status_name,
            .out_ov = (struct TdoOverlap) {
                .overlapped = {0},
                .status = TDO_PIPE_IDLE,
                .kind = TDO_LOG_OUT,
            },
            .err_ov = (struct TdoOverlap) {
                .overlapped = {0},
                .status = TDO_PIPE_IDLE,
                .kind = TDO_LOG_ERR,
            },
            .status_ov = (struct TdoOverlap) {
                .overlapped = {0},
                .status = TDO_PIPE_IDLE,
                .kind = TDO_LOG_STATUS,
            },
            .active = false,
        };

        // set up IOCP
        CreateIoCompletionPort(h_out, status->iocp, (ULONG_PTR) &status->runs[i], 0);
        CreateIoCompletionPort(h_err, status->iocp, (ULONG_PTR) &status->runs[i], 0);
        CreateIoCompletionPort(h_status, status->iocp, (ULONG_PTR) &status->runs[i], 0);
    }

    result = TDO_ERROR_OK;

    if (false) {
        error_named_pipe_setup:
        for (size_t i = 0; i < args.processes; i++) {
            struct TdoRun run = status->runs[i];
            if (run.out.fd != INVALID_HANDLE_VALUE) CloseHandle(run.out.fd);
            if (run.err.fd != INVALID_HANDLE_VALUE) CloseHandle(run.err.fd);
            if (run.status.fd != INVALID_HANDLE_VALUE) CloseHandle(run.status.fd);
        }
        error_job_settings:
        CloseHandle(status->job);
        error_job_setup:
        CloseHandle(status->iocp);
        tdo_arena_state_set(arena, state);
    }
    error_setup:
    return result;
}

void tdo_run_status_deinit(struct TdoRunStatus status, struct TdoArguments args) {
    for (size_t i = 0; i < args.processes; i++) {
        struct TdoRun run = status.runs[i];
        CloseHandle(run.out.fd);
        CloseHandle(run.err.fd);
        CloseHandle(run.status.fd);
    }
    CloseHandle(status.job);
    CloseHandle(status.iocp);
}
