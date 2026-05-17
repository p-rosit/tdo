#include <stdio.h>
#include <stdlib.h>

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    #define EXPORT
    #include <signal.h>

    void test_stop(void) {
        fprintf(stdout, "Before stop\n");
        fflush(NULL);
        raise(SIGSTOP);
        fprintf(stdout, "after stop\n");
        fflush(NULL);
    }
#elif defined(_WIN32)
    #define EXPORT __declspec(dllexport)
#endif

EXPORT void test_success(void) {
}

EXPORT void test_success_with_stdout(void) {
    fprintf(stdout, "Printed\n");
}

EXPORT void test_success_with_other_stdout(void) {
    fprintf(stdout, "other\n");
}

EXPORT void test_success_with_stderr(void) {
    fprintf(stderr, "Other thing\n");
}

EXPORT void test_early_exit(void) {
    exit(4);
}

EXPORT void test_aborts(void) {
    abort();
}

EXPORT void test_print_forever(void) {
    while (1) {
        fprintf(stdout, "I am printing forever!\n");
    }
}

EXPORT void test_timeout(void) {
    fprintf(stdout, "Some output\n"); fflush(stdout);
    fprintf(stderr, "Some error\n"); fflush(stderr);

    volatile unsigned int x = 0;
    while (1) {
        x++;
    }
}

