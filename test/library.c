#include <stdio.h>
#include <stdlib.h>

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    #define EXPORT
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
