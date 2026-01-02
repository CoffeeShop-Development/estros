#ifndef DYNARR_H
#define DYNARR_H 1

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

#endif
