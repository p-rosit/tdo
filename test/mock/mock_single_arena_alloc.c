#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "../../src/arena.h"

static int arena_alloc_fail_index = 0;
static int arena_alloc_amount = 0;
static bool alloc_failed = false;
static bool resize_failed = false;

void *tdo_mock_tdo_arena_alloc(struct TdoArena *arena, size_t type_size, size_t amount) {
    arena_alloc_amount += arena_alloc_amount <= arena_alloc_fail_index + 1;
    if (arena_alloc_amount > arena_alloc_fail_index && !alloc_failed) {
        alloc_failed = true;
        return NULL;
    }
    return tdo_arena_alloc(arena, type_size, amount);
}

bool tdo_mock_tdo_arena_resize(struct TdoArena *arena, void *allocation, size_t type_size, size_t amount) {
    arena_alloc_amount += arena_alloc_amount <= arena_alloc_fail_index + 1;
    if (arena_alloc_amount > arena_alloc_fail_index && !resize_failed) {
        resize_failed = true;
        return false;
    }
    return tdo_arena_resize(arena, allocation, type_size, amount);
}

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
        arena_alloc_fail_index = INT_MAX - 2;
        return tdo_runner_main(argc, argv);
    }

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments, expected max amount of calls to tdo_arena_alloc\n");
        abort();
    }
    if (strcmp(argv[argc - 2], "--mock-arena-alloc-max") != 0) {
        fprintf(stderr, "Invalid final argument, expected '--mock-arena-alloc-max', got '%s'\n", argv[argc - 2]);
        abort();
    }

    arena_alloc_fail_index = atoi(argv[argc - 1]);
    if (arena_alloc_fail_index <= 0 && strcmp(argv[argc - 1], "0") != 0) {
        fprintf(stderr, "Expected number of calls to tdo_arena_alloc, got '%s'\n", argv[argc - 1]);
        abort();
    }

    return tdo_runner_main(argc - 2, argv);
}
