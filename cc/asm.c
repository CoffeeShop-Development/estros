#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "util.h"
#include "state.h"
#include "asm.h"
#include "ssa.h"

enum cc_asm_register_category {
    CC_ASM_REGISTER_CATEGORY_INTEGER,
    CC_ASM_REGISTER_CATEGORY_FLOAT,
    CC_ASM_REGISTER_CATEGORY_STACK,
    CC_ASM_REGISTER_CATEGORY_BASE_POINTER,
    CC_ASM_REGISTER_CATEGORY_RETURN_ADDRESS,
    CC_ASM_REGISTER_CATEGORY_THREAD_POINTER,
};

#define CC_ASM_REGISTER_LIST \
    CC_ASM_REGISTER_ELEM(F0, f0) \
    CC_ASM_REGISTER_ELEM(F1, f1) \
    CC_ASM_REGISTER_ELEM(F2, f2) \
    CC_ASM_REGISTER_ELEM(F3, f3) \
    CC_ASM_REGISTER_ELEM(F4, f4) \
    CC_ASM_REGISTER_ELEM(F5, f5) \
    CC_ASM_REGISTER_ELEM(F6, f6) \
    CC_ASM_REGISTER_ELEM(F7, f7) \
    CC_ASM_REGISTER_ELEM(F8, f8) \
    CC_ASM_REGISTER_ELEM(F9, f9) \
    CC_ASM_REGISTER_ELEM(F10, f10) \
    CC_ASM_REGISTER_ELEM(F11, f11) \
    CC_ASM_REGISTER_ELEM(F12, f12) \
    CC_ASM_REGISTER_ELEM(F13, f13) \
    CC_ASM_REGISTER_ELEM(F14, f14) \
    CC_ASM_REGISTER_ELEM(F15, f15) \
    CC_ASM_REGISTER_ELEM(T0, t0) \
    CC_ASM_REGISTER_ELEM(T1, t1) \
    CC_ASM_REGISTER_ELEM(T2, t2) \
    CC_ASM_REGISTER_ELEM(T3, t3) \
    CC_ASM_REGISTER_ELEM(T4, t4) \
    CC_ASM_REGISTER_ELEM(T5, t5) \
    CC_ASM_REGISTER_ELEM(T6, t6) \
    CC_ASM_REGISTER_ELEM(T7, t7) \
    CC_ASM_REGISTER_ELEM(A0, a0) \
    CC_ASM_REGISTER_ELEM(A1, a1) \
    CC_ASM_REGISTER_ELEM(A2, a2) \
    CC_ASM_REGISTER_ELEM(A3, a3) \
    CC_ASM_REGISTER_ELEM(RA, ra) \
    CC_ASM_REGISTER_ELEM(BP, bp) \
    CC_ASM_REGISTER_ELEM(SP, sp) \
    CC_ASM_REGISTER_ELEM(TP, tp)
    

typedef enum cc_asm_register {
#define CC_ASM_REGISTER_ELEM(NAME, ...) CC_ASM_REGISTER_##NAME,
    CC_ASM_REGISTER_LIST
#undef CC_ASM_REGISTER_ELEM
    /* First spill aka. virtual register */
    CC_ASM_REGISTER_SPILL,
} cc_asm_register_t;

typedef struct cc_asm_state {
    cc_state_t *state;
    DYNARR_DECL(asm_out, char);
    DYNARR_DECL(reg_info, cc_ssa_argument_t);
} cc_asm_state_t;

static const char *asm_regname[CC_ASM_REGISTER_SPILL] = {
#define CC_ASM_REGISTER_ELEM(NAME, CNAME) "$"#CNAME,
    CC_ASM_REGISTER_LIST
#undef CC_ASM_REGISTER_ELEM
};

static void cc_asm_write(cc_asm_state_t *as, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int r = vsnprintf(as->asm_out + as->n_asm_out, BUFSIZ, fmt, va);
    as->n_asm_out += r;
    va_end(va);
}

static bool cc_asm_is_fixed_reg(cc_asm_register_t r) {
    return (r >= CC_ASM_REGISTER_A0 || r <= CC_ASM_REGISTER_A3)
        || r == CC_ASM_REGISTER_RA || r == CC_ASM_REGISTER_TP
        || r == CC_ASM_REGISTER_BP || r == CC_ASM_REGISTER_SP;
}
static cc_asm_register_t cc_asm_find_free(cc_asm_state_t *as, enum cc_asm_register_category cat) {
    /* Integer from T0 to T7 */
    for (size_t i = CC_ASM_REGISTER_T0; cat == CC_ASM_REGISTER_CATEGORY_INTEGER && i < CC_ASM_REGISTER_T7; ++i)
        if (as->reg_info[i].id == 0)
            return i;
    /* Float from F0 to F15 */
    for (size_t i = CC_ASM_REGISTER_F0; cat == CC_ASM_REGISTER_CATEGORY_FLOAT && i < CC_ASM_REGISTER_F15; ++i)
        if (as->reg_info[i].id == 0)
            return i;
    /* Spills */
    for (size_t i = CC_ASM_REGISTER_SPILL; i < as->n_reg_info; ++i)
        if (as->reg_info[i].id == 0)
            return i;
    ++as->n_reg_info;
    return as->n_reg_info - 1;
}
static cc_asm_register_t cc_asm_regalloc(cc_asm_state_t *as, enum cc_asm_register_category cat, cc_ssa_argument_t arg) {
    cc_asm_register_t reg = cc_asm_find_free(as, cat);
    as->reg_info[reg] = arg;
    return reg;
}
static void cc_asm_regfree(cc_asm_state_t *as, cc_asm_register_t reg) {
    assert(as->reg_info[reg].id != 0);
    if (!cc_asm_is_fixed_reg(reg))
        as->reg_info[reg] = (cc_ssa_argument_t){0};
}
static cc_asm_register_t cc_asm_find_used(cc_asm_state_t *as, cc_ssa_argument_t arg) {
    for (size_t i = 0; i < as->n_reg_info; ++i)
        if (as->reg_info[i].id == arg.id)
            return i;
    abort();
}

static void cc_asm_write_raw_data(cc_asm_state_t *as, cc_raw_data_view_t raw) {
    size_t i = 0;
    for (; i + sizeof(uint64_t) <= raw.len; i += sizeof(uint64_t))
        cc_asm_write(as, "\t.quad %016" PRIx64 "\n", *(uint64_t const *)(as->state->raw_data + raw.pos + i));
    for (; i + sizeof(uint32_t) <= raw.len; i += sizeof(uint32_t))
        cc_asm_write(as, "\t.long %08" PRIx32 "\n", *(uint32_t const *)(as->state->raw_data + raw.pos + i));
    for (; i + sizeof(uint16_t) <= raw.len; i += sizeof(uint16_t))
        cc_asm_write(as, "\t.word %04" PRIx16 "\n", *(uint16_t const *)(as->state->raw_data + raw.pos + i));
    for (; i + sizeof(uint8_t) <= raw.len; i += sizeof(uint8_t))
        cc_asm_write(as, "\t.byte %02" PRIx8 "\n", *(uint8_t const *)(as->state->raw_data + raw.pos + i));
}

static void cc_asm_generic_binop(cc_asm_state_t *as, cc_ssa_function_t *func, size_t offset, const char *insn) {
    cc_ssa_token_t const *tok = as->state->ssa_tokens + func->tokens.pos + offset;
    cc_asm_register_t res = cc_asm_regalloc(as, CC_ASM_REGISTER_CATEGORY_INTEGER, tok->result);
    cc_asm_register_t arg0 = cc_asm_find_used(as, tok->data.args[0]);
    cc_asm_register_t arg1 = cc_asm_find_used(as, tok->data.args[1]);
    cc_asm_write(as, "\t%s %s,%s,%s\n", insn, asm_regname[res], asm_regname[arg0], asm_regname[arg1]);
    if (cc_ssa_is_last_use(as->state, func, offset + 1, tok->data.args[0]))
        cc_asm_regfree(as, arg0);
    if (cc_ssa_is_last_use(as->state, func, offset + 1, tok->data.args[1]))
        cc_asm_regfree(as, arg1);
}

static void cc_asm_lower_func(cc_asm_state_t *as, cc_ssa_function_t *func) {
    cc_asm_write(as, ".section .text\n");
    cc_asm_write(as, "%s:\n", cc_strview_get(as->state, func->name));
    for (size_t i = 0; i < func->tokens.len; ++i) {
        cc_ssa_token_t const *tok = as->state->ssa_tokens + func->tokens.pos + i;
        switch (tok->type) {
        case CC_SSA_TOK_IDENTITY: {
            cc_asm_register_t reg = cc_asm_regalloc(as, CC_ASM_REGISTER_CATEGORY_INTEGER, tok->result);
            cc_asm_register_t param_reg = CC_ASM_REGISTER_A0 + tok->data.args[0].id - CC_SSA_ID_PARAM(0);
            if (param_reg >= CC_ASM_REGISTER_A0 && param_reg <= CC_ASM_REGISTER_A3) {
                if (tok->data.args[0].width == CC_SSA_WIDTH_SET(32)) {
                    cc_asm_write(as, "\tor %s,%s,%s,0\n", asm_regname[reg], asm_regname[param_reg], asm_regname[param_reg]);
                } else if (tok->data.args[0].width == CC_SSA_WIDTH_SET(16)) {
                    cc_asm_write(as, "\tor %s,%s,%s,0\n", asm_regname[reg], asm_regname[param_reg], asm_regname[param_reg]);
                } else if (tok->data.args[0].width == CC_SSA_WIDTH_SET(8)) {
                    cc_asm_write(as, "\tor %s,%s,%s,0\n", asm_regname[reg], asm_regname[param_reg], asm_regname[param_reg]);
                    cc_asm_write(as, "\tand %s,%s,%s,0xff\n", asm_regname[reg], asm_regname[reg], asm_regname[reg]);
                } else {
                    abort();
                }
            }
            break;
        }
        case CC_SSA_TOK_RAW_DATA: {
            cc_asm_register_t reg = cc_asm_regalloc(as, CC_ASM_REGISTER_CATEGORY_INTEGER, tok->result);
            if (tok->result.width == CC_SSA_WIDTH_SET(32)) {
                cc_asm_write(as, "\tldl %s,D%u\n", asm_regname[reg], (unsigned)tok->result.id);
            } else if (tok->result.width == CC_SSA_WIDTH_SET(16)) {
                cc_asm_write(as, "\tldw %s,D%u\n", asm_regname[reg], (unsigned)tok->result.id);
            } else {
                abort();
            }
            break;
        }
        case CC_SSA_TOK_ADD: cc_asm_generic_binop(as, func, i, "add"); break;
        case CC_SSA_TOK_SUB: cc_asm_generic_binop(as, func, i, "sub"); break;
        case CC_SSA_TOK_MUL: cc_asm_generic_binop(as, func, i, "mul"); break;
        case CC_SSA_TOK_DIV: cc_asm_generic_binop(as, func, i, "div"); break;
        case CC_SSA_TOK_REM: cc_asm_generic_binop(as, func, i, "rem"); break;
        case CC_SSA_TOK_LSHIFT: cc_asm_generic_binop(as, func, i, "shl"); break;
        case CC_SSA_TOK_RSHIFT: cc_asm_generic_binop(as, func, i, "shr"); break;
        case CC_SSA_TOK_XOR: cc_asm_generic_binop(as, func, i, "xor"); break;
        case CC_SSA_TOK_OR: cc_asm_generic_binop(as, func, i, "or"); break;
        case CC_SSA_TOK_AND: cc_asm_generic_binop(as, func, i, "and"); break;
        case CC_SSA_TOK_RETURN: {
            cc_asm_write(as, "\tret\n");
            break;
        }
        default:
            break;
        }
    }

    cc_asm_write(as, ".section .data\n");
    for (size_t i = 0; i < func->tokens.len; ++i) {
        cc_ssa_token_t const *tok = as->state->ssa_tokens + func->tokens.pos + i;
        if (tok->type == CC_SSA_TOK_RAW_DATA) {
            cc_asm_write(as, "D%u:\n", tok->result.id);
            cc_asm_write_raw_data(as, tok->data.raw);
        }
    }
}

static void cc_asm_init(cc_asm_state_t *as) {
    DYNARR_INIT(*as, asm_out);
    DYNARR_INIT(*as, reg_info);
}

static void cc_asm_finish(cc_asm_state_t *as) {
    as->asm_out[as->n_asm_out] = '\0';
    printf("%s\n", as->asm_out);
    DYNARR_FINISH(*as, asm_out);
    DYNARR_FINISH(*as, reg_info);
}

void cc_asm_lower_ssa(cc_state_t *state) {
    cc_asm_state_t as;
    as.state = state;

    cc_asm_init(&as);
    for (size_t i = 0; i < state->n_ssa_funcs; ++i) {
        /* Builtin registers */
        memset(as.reg_info, 0, sizeof(cc_ssa_argument_t) * CC_ASM_REGISTER_SPILL);
        as.n_reg_info = CC_ASM_REGISTER_SPILL;

        /* Parameters? */
        for (size_t i = 0; i < 4; ++i) {
            as.reg_info[CC_ASM_REGISTER_A0 + i].id = CC_SSA_ID_PARAM(i);
            as.reg_info[CC_ASM_REGISTER_A0 + i].width = CC_SSA_WIDTH_SET(32);
        }

        cc_asm_lower_func(&as, state->ssa_funcs + i);
    }
    cc_asm_finish(&as);
}
