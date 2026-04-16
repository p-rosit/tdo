#ifndef TDO_ARENA_H
#define TDO_ARENA_H

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
    void const *last_allocation;
};

void *tdo_arena_alloc(struct TdoArena *arena, size_t type_size, size_t amount);

#endif