#include <stdio.h>
#include <stdlib.h>

void test_success(void) {
}

void test_success_with_stdout(void) {
    fprintf(stdout, "Printed\n");
}

void test_success_with_other_stdout(void) {
    fprintf(stdout, "other\n");
}

void test_success_with_stderr(void) {
    fprintf(stderr, "Other thing\n");
}

void test_early_exit(void) {
    exit(4);
}

void test_aborts(void) {
    abort();
}
