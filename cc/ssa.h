#include "ast.h"
#ifndef SSA_H
#define SSA_H 1

#include <assert.h>
#include <stdbool.h>
#include "util.h"

#define CC_SSA_MAX_FUNC_ARGS 8

#define CC_SSA_WIDTH_SET(N) \
    (((N) == 0) ? 1 : ((N) == 1) ? 2 : ((N) == 2) ? 3 \
    : ((N) == 4) ? 4 : ((N) == 8) ? 5 : ((N) == 16) ? 6 \
    : ((N) == 32) ? 7 : ((N) == 64) ? 8 : ((N) == 128) ? 9 \
    : ((N) == 256) ? 10 : ((N) == 512) ? 11 : ((N) == 1024) ? 12 \
    : ((N) == 2048) ? 13 : ((N) == 4096) ? 14 : ((N) == 8192) ? 15 \
    : assert(false && "invalid ssa width"), 0)
#define CC_SSA_WIDTH_GET(N) (1 << (N))

typedef struct cc_state cc_state_t;

typedef struct cc_raw_data_view {
    unsigned _BitInt(24) pos;
    unsigned _BitInt(8) len;
} cc_raw_data_view_t;
typedef struct cc_ssa_token_view {
    unsigned _BitInt(24) pos;
    unsigned _BitInt(8) len;
} cc_ssa_token_view_t;
typedef struct cc_ssa_argument {
    unsigned _BitInt(12) id;
    /* 0000 = 1 -> 0001 = 2 -> ... -> 1001 = 512 */
    unsigned _BitInt(4) width;
} cc_ssa_argument_t;
typedef struct cc_ssa_token {
    cc_ssa_argument_t result;
    union {
        /* For call, the first arg is the func-ptr */
        cc_ssa_argument_t args[1 + CC_SSA_MAX_FUNC_ARGS];
        cc_raw_data_view_t raw;
    } data;
    enum cc_ssa_token_type {
        CC_SSA_TOK_INVALID = 0,
        /* return %0 */
        CC_SSA_TOK_RETURN,
        /* %0 = call <function-ptr %1> (%2, %3, ..., %n) */
        CC_SSA_TOK_CALL,
        /* if %0 then <label %1> else <label %2> */
        CC_SSA_TOK_BRANCH,
        /* %0 = phi [%1, %2, ..., %n] */
        CC_SSA_TOK_PHI,
        /* %0 = %1 */
        CC_SSA_TOK_IDENTITY,
        CC_SSA_TOK_NOT,
        /* %0 = %1 op %2 */
        CC_SSA_TOK_ADD,
        CC_SSA_TOK_SUB,
        CC_SSA_TOK_MUL,
        CC_SSA_TOK_DIV,
        CC_SSA_TOK_REM,
        CC_SSA_TOK_LSHIFT,
        CC_SSA_TOK_RSHIFT,
        CC_SSA_TOK_AND,
        CC_SSA_TOK_XOR,
        CC_SSA_TOK_OR,
        /* %0 = %1 float-op %2 */
        CC_SSA_TOK_FADD,
        CC_SSA_TOK_FSUB,
        CC_SSA_TOK_FMUL,
        CC_SSA_TOK_FDIV,
        /* %0 = <raw data> */
        CC_SSA_TOK_RAW_DATA = 0x80,
    } type;
} cc_ssa_token_t;
typedef struct cc_ssa_function {
    cc_strview_t name;
    cc_ssa_token_view_t tokens;
} cc_ssa_function_t;

void cc_ssa_lower(cc_state_t *state);
bool cc_ssa_is_used_in(cc_state_t *state, cc_ssa_token_t const *tok, cc_ssa_argument_t arg);
bool cc_ssa_is_last_use(cc_state_t *state, cc_ssa_function_t *func, size_t offset, cc_ssa_argument_t arg);
void cc_ssa_from_ast(cc_state_t *state, cc_ast_node_ref_t root);

/* Null aka. eliminated (literally nothing) */
#define CC_SSA_ID_NULL 0
/* Return value */
#define CC_SSA_ID_RETVAL 1
/* Arguments to functions are taken from 2 to 127 */
#define CC_SSA_ID_ARG(N) ((N) + 2)
/* First non-hardcoded temporal */
#define CC_SSA_FIRST_TEMPORAL 128

#endif
