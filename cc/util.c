#include <stdio.h>
#include <string.h>
#include "util.h"
#include "state.h"

const char *cc_strview_get(cc_state_t *state, cc_strview_t s) {
    static char buf[BUFSIZ];
    memcpy(buf, state->data + s.pos, s.len);
    buf[s.len] = '\0';
    return buf;
}
