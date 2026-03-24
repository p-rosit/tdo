#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #include <stddef.h>
    #define TDO_ARENA_ALIGNMENT alignof(max_align_t)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #include <stddef.h>
    #define TDO_ARENA_ALIGNMENT _Alignof(max_align_t)
#else
    #include <stddef.h>

    union MaxAlign {
        void *p;
        long l;
        double d;
        long double ld;
        long long ll;
        void (*f)(void);
    };

    struct MaxAlignHelper {
        char a;
        union MaxAlign b;
    };

    #define TDO_ARENA_ALIGNMENT offsetof(struct MaxAlignHelper, b)
#endif

struct TdoArenaNode {
    struct TdoArenaNode *next;
    char *current;
    char *end;
};

struct TdoArena {
    struct TdoArenaNode *first;
    struct TdoArenaNode *latest;
};

struct TdoArena *tdo_arena_init(size_t initial_capacity) {
    size_t header_size = sizeof(struct TdoArena) + sizeof(struct TdoArenaNode);

    if (initial_capacity > SIZE_MAX - header_size) return NULL;
    struct TdoArena *arena = malloc(header_size + initial_capacity);
    if (arena == NULL) return NULL;

    struct TdoArenaNode *node = (struct TdoArenaNode*) ((char*) arena + sizeof(struct TdoArena));
    char *buffer = (char*) node + sizeof(struct TdoArenaNode);

    *arena = (struct TdoArena) {
        .first = node,
        .latest = node,
    };

    *node = (struct TdoArenaNode) {
        .next = NULL,
        .current = buffer,
        .end = buffer + initial_capacity,
    };
    return arena;
}

void tdo_arena_deinit(struct TdoArena *arena) {
    struct TdoArenaNode *node = arena->first->next;
    free(arena);

    for (struct TdoArenaNode *next = node->next; node != NULL; node = next) {
        next = node->next;
        free(node);
    }
}

void tdo_arena_state_clear(struct TdoArena *arena) {
    for (struct TdoArenaNode *node = arena->first; node != NULL; node = node->next) {
        node->current = (char*) node + sizeof(struct TdoArenaNode);
    }
}

struct TdoArenaState {
    struct TdoArenaNode *const node;
    char *const current;
};

struct TdoArenaState tdo_arena_state_get(struct TdoArena *arena) {
    return (struct TdoArenaState) {
        .node = arena->latest,
        .current = arena->latest->current,
    };
}

void tdo_arena_state_set(struct TdoArena *arena, struct TdoArenaState state) {
    struct TdoArenaNode *node = arena->first;

    for (; node != NULL; node = node->next) {
        if (node == state.node) {
            if (node->current < state.current) {
                char *start = (char*) node + sizeof(struct TdoArenaNode);
                fprintf(stderr, "Cannot reset arena state forward: current=%zu, new=%zu\n", node->current - start, state.current - start);
                abort();
            }

            node->current = state.current;
            break;
        }

        node->current = (char*) node + sizeof(struct TdoArenaNode);
    }

    tdo_arena_state_clear(&(struct TdoArena) { .first = node, .latest = NULL });
}

char *tdo_buffer_alloc(char *buffer_start, char *buffer_end, size_t bytes) {
    if (buffer_end < buffer_start) {
        fprintf(stderr, "Buffer start has gone past end\n");
        abort();
    }

    size_t current_offset = ((uintptr_t) buffer_start) % TDO_ARENA_ALIGNMENT;
    size_t remaining_offset = (TDO_ARENA_ALIGNMENT - current_offset) % TDO_ARENA_ALIGNMENT;

    if ((uintptr_t) buffer_start > UINTPTR_MAX - remaining_offset) return NULL; // overflow due to alignment
    char *aligned_data = buffer_start + remaining_offset;
    if (buffer_end < aligned_data) return NULL; // just the alignment went past the end

    size_t remaining_capacity = (size_t) (buffer_end - aligned_data);
    if (remaining_capacity < bytes) return NULL; // allocation does not fit
    return aligned_data;
}

void *tdo_arena_alloc(struct TdoArena *arena, size_t type_size, size_t amount) {
    if (amount > SIZE_MAX / type_size) return NULL;
    size_t total_bytes = type_size * amount;

    for (struct TdoArenaNode *node = arena->latest; node != NULL; node = node->next) {
        char *allocation = tdo_buffer_alloc(node->current, node->end, total_bytes);
        if (allocation != NULL) {
            arena->latest = node;
            node->current = allocation + total_bytes;
            return allocation;
        }
        node->current = node->end;
    }

    if (arena->latest->next != NULL && arena->latest->current == arena->latest->end) {
        fprintf(stderr, "Expected arena to be exhausted\n");
        abort();
    }

    size_t capacity = (char*) arena->latest->end - ((char*) arena->latest + sizeof(struct TdoArenaNode));
    if (capacity > SIZE_MAX / 2)


        
    void *allocation = tdo_buffer_alloc(arena->latest)

    return NULL;
}
