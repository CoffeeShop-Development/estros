#ifndef STATE_H
#define STATE_H 1

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "lexer.h"
#include "dynarr.h"
#include "ssa.h"
#include "strview.h"
#include "ast.h"
#include "ssa.h"
#include "lexer.h"

typedef struct cc_state {
    void *data;
    size_t n_data;
    /* Lexer */
    DYNARR_DECL(tokens, cc_token_t);
    size_t cur_tok;
    /* Parser */
    DYNARR_DECL(nodes, cc_ast_node_t);
    /* SSA */
    DYNARR_DECL(ssa_funcs, cc_ssa_function_t);
    DYNARR_DECL(ssa_tokens, cc_ssa_token_t);
    DYNARR_DECL(raw_data, char);
} cc_state_t;

#define CC_PUSH_RAW_DATA(NAME, TYPE) \
    static inline cc_raw_data_view_t cc_push_raw_data_##NAME(cc_state_t *state, TYPE v) { \
        cc_raw_data_view_t view; \
        *(TYPE*)(state->raw_data + state->n_raw_data) = v; \
        state->n_raw_data += sizeof(TYPE); \
        view.pos = state->n_raw_data - sizeof(TYPE); \
        view.len = sizeof(TYPE); \
        return view; \
    }
CC_PUSH_RAW_DATA(u8, uint8_t);
CC_PUSH_RAW_DATA(u16, uint16_t);
CC_PUSH_RAW_DATA(u32, uint32_t);
CC_PUSH_RAW_DATA(u64, uint64_t);
CC_PUSH_RAW_DATA(s8, int8_t);
CC_PUSH_RAW_DATA(s16, int16_t);
CC_PUSH_RAW_DATA(s32, int32_t);
CC_PUSH_RAW_DATA(s64, int64_t);
CC_PUSH_RAW_DATA(f32, float);
CC_PUSH_RAW_DATA(f64, double);
#undef CC_PUSH_RAW_DATA

#endif
