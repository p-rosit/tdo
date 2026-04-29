#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int tdo_runner_main(int argc, char **argv);
int main(int argc, char **argv) {
    bool is_child = false;
    for (size_t i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--internal-status") == 0) {
            is_child = true;
            break;
        }
    }
    if (is_child) {
        fprintf(stderr, "Crashing child before main...\n");
        fflush(NULL);
        abort();
    }
    return tdo_runner_main(argc, argv);
}
