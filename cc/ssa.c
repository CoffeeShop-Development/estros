#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include "ssa.h"
#include "state.h"
#include "asm.h"

static const char *cc_ssa_get_token_name(enum cc_ssa_token_type t) {
    switch (t) {
    case CC_SSA_TOK_CALL: return "call";
    case CC_SSA_TOK_BRANCH: return "branch";
    case CC_SSA_TOK_PHI: return "phi";
    case CC_SSA_TOK_IDENTITY: return "identity";
    case CC_SSA_TOK_NOT: return "not";
    case CC_SSA_TOK_ADD: return "add";
    case CC_SSA_TOK_SUB: return "sub";
    case CC_SSA_TOK_MUL: return "mul";
    case CC_SSA_TOK_DIV: return "div";
    case CC_SSA_TOK_REM: return "rem";
    case CC_SSA_TOK_LSHIFT: return "lsh";
    case CC_SSA_TOK_RSHIFT: return "rsh";
    case CC_SSA_TOK_AND: return "and";
    case CC_SSA_TOK_XOR: return "xor";
    case CC_SSA_TOK_OR: return "or";
    case CC_SSA_TOK_FADD: return "fadd";
    case CC_SSA_TOK_FSUB: return "fsub";
    case CC_SSA_TOK_FMUL: return "fmul";
    case CC_SSA_TOK_FDIV: return "fdiv";
    case CC_SSA_TOK_RAW_DATA: return "raw data";
    default: return "invalid";
    }
}

static void cc_ssa_print_func(cc_state_t *state, cc_ssa_function_t *func) {
    printf("<func>:\n");
    for (size_t i = 0; i < func->tokens.len; ++i) {
        cc_ssa_token_t const *tok = state->ssa_tokens + func->tokens.pos + i;
        switch (tok->type) {
        case CC_SSA_TOK_IDENTITY:
            printf("\t%%%u(%u) = %s %%%u(%u)\n",
                (unsigned)tok->result.id, CC_SSA_WIDTH_GET(tok->result.width),
                cc_ssa_get_token_name(tok->type),
                (unsigned)tok->data.args[0].id, CC_SSA_WIDTH_GET(tok->data.args[0].width)
            );
            break;
        case CC_SSA_TOK_CALL:
            printf("\t%%%u(%u) = %s <func-ptr %%%u(%u)>(",
                (unsigned)tok->result.id, CC_SSA_WIDTH_GET(tok->result.width),
                cc_ssa_get_token_name(tok->type),
                (unsigned)tok->data.args[0].id, CC_SSA_WIDTH_GET(tok->data.args[0].width)
            );
            for (size_t i = 1; i < CC_SSA_MAX_FUNC_ARGS; ++i)
                printf("%%%u(%u), ", (unsigned)tok->data.args[i].id, CC_SSA_WIDTH_GET(tok->data.args[i].width));
            printf(")\n");
            break;
        case CC_SSA_TOK_RAW_DATA: {
            cc_raw_data_view_t raw = tok->data.raw;
            if (raw.len == sizeof(uint64_t)) {
                printf("\t%%%u(%u) = %s <raw %016" PRIx64 ": %u,%u>\n",
                    (unsigned)tok->result.id, CC_SSA_WIDTH_GET(tok->result.width),
                    cc_ssa_get_token_name(tok->type),
                    *(uint64_t const *)(state->raw_data + raw.pos),
                    (unsigned)raw.pos, (unsigned)raw.len
                );
            } else if (raw.len == sizeof(uint32_t)) {
                printf("\t%%%u(%u) = %s <raw %08" PRIx32 ": %u,%u>\n",
                    (unsigned)tok->result.id, CC_SSA_WIDTH_GET(tok->result.width),
                    cc_ssa_get_token_name(tok->type),
                    *(uint32_t const *)(state->raw_data + raw.pos),
                    (unsigned)raw.pos, (unsigned)raw.len
                );
            } else if (raw.len == sizeof(uint16_t)) {
                printf("\t%%%u(%u) = %s <raw %04" PRIx16 ": %u,%u>\n",
                    (unsigned)tok->result.id, CC_SSA_WIDTH_GET(tok->result.width),
                    cc_ssa_get_token_name(tok->type),
                    *(uint16_t const *)(state->raw_data + raw.pos),
                    (unsigned)raw.pos, (unsigned)raw.len
                );
            } else if (raw.len == sizeof(uint8_t)) {
                printf("\t%%%u(%u) = %s <raw %02" PRIx8 ": %u,%u>\n",
                    (unsigned)tok->result.id, CC_SSA_WIDTH_GET(tok->result.width),
                    cc_ssa_get_token_name(tok->type),
                    *(uint8_t const *)(state->raw_data + raw.pos),
                    (unsigned)raw.pos, (unsigned)raw.len
                );
            } else {
                printf("\t%%%u(%u) = %s <raw %u,%u>\n",
                    (unsigned)tok->result.id, CC_SSA_WIDTH_GET(tok->result.width),
                    cc_ssa_get_token_name(tok->type),
                    (unsigned)raw.pos, (unsigned)raw.len
                );
            }
            break;
        }
        default:
            printf("\t%%%u(%u) = %%%u(%u) %s %%%u(%u)\n",
                (unsigned)tok->result.id, CC_SSA_WIDTH_GET(tok->result.width),
                (unsigned)tok->data.args[0].id, CC_SSA_WIDTH_GET(tok->data.args[0].width),
                cc_ssa_get_token_name(tok->type),
                (unsigned)tok->data.args[1].id, CC_SSA_WIDTH_GET(tok->data.args[1].width)
            );
            break;
        }
    }
}

void cc_ssa_dump(cc_state_t *state) {
    for (size_t i = 0; i < state->n_ssa_funcs; ++i)
        cc_ssa_print_func(state, state->ssa_funcs + i);
}

/* If the argument arg is used within the token itself */
bool cc_ssa_is_used_in(cc_state_t *state, cc_ssa_token_t const *tok, cc_ssa_argument_t arg) {
    switch (tok->type) {
    case CC_SSA_TOK_RAW_DATA:
        return tok->result.id == arg.id;
    default:
        for (size_t i = 0; i < CC_SSA_MAX_FUNC_ARGS; ++i)
            if (tok->data.args[i].id == arg.id)
                return true;
        return tok->result.id == arg.id;
    }
}
/* If the argument at this given offset is last used? */
bool cc_ssa_is_last_use(cc_state_t *state, cc_ssa_function_t *func, size_t offset, cc_ssa_argument_t arg) {
    for (size_t i = offset; i < func->tokens.len; ++i)
        if (cc_ssa_is_used_in(state, state->ssa_tokens + func->tokens.pos + i, arg))
            return false;
    return true;
}
