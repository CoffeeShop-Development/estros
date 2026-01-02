#ifndef STRVIEW_H
#define STRVIEW_H 1

#include <assert.h>
#include <sys/mman.h>

#define DYNARR_MAX_SIZE 268435456ULL

#define DYNARR_DECL(NAME, TYPE) \
    TYPE *NAME; \
    size_t n_##NAME;

#define DYNARR_INIT(SU, NAME) \
    do { \
        (SU).NAME = mmap(NULL, DYNARR_MAX_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0); \
        (SU).n_##NAME = 0; \
        assert((SU).NAME != MAP_FAILED); \
    } while (0);

#define DYNARR_FINISH(SU, NAME) \
    munmap((SU).NAME, DYNARR_MAX_SIZE);

typedef struct cc_strview {
    unsigned _BitInt(24) pos;
    unsigned _BitInt(8) len;
} cc_strview_t;

typedef struct cc_state cc_state_t;
const char *cc_strview_get(cc_state_t *state, cc_strview_t s);

#endif
