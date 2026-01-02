#ifndef STRVIEW_H
#define STRVIEW_H 1

typedef struct cc_strview {
    unsigned _BitInt(24) pos;
    unsigned _BitInt(8) len;
} cc_strview_t;

#endif
