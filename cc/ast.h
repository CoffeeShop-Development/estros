#ifndef AST_H
#define AST_H 1

#include <limits.h>
#include <stddef.h>
#include "strview.h"

typedef struct cc_ast_node_ref {
    _BitInt(16) value;
} cc_ast_node_ref_t;
typedef struct cc_ast_node {
    union cc_ast_node_data {
        cc_strview_t type_name;
        cc_strview_t var_name;
        struct {
            cc_strview_t name;
            cc_ast_node_ref_t return_type;
            cc_ast_node_ref_t *childs;
            size_t n_childs;
            enum cc_ast_type_flags {
                CC_AST_TYPE_FLAGS_VOID,
                CC_AST_TYPE_FLAGS_BOOL,
                CC_AST_TYPE_FLAGS_CHAR,
                CC_AST_TYPE_FLAGS_SHORT,
                CC_AST_TYPE_FLAGS_INT,
                CC_AST_TYPE_FLAGS_LONG,
                CC_AST_TYPE_FLAGS_LONG_LONG,
                CC_AST_TYPE_FLAGS_FLOAT,
                CC_AST_TYPE_FLAGS_DOUBLE,
                CC_AST_TYPE_FLAGS_COMPLEX,
                CC_AST_TYPE_FLAGS_IMAGINARY,
                CC_AST_TYPE_FLAGS_DECIMAL32,
                CC_AST_TYPE_FLAGS_DECIMAL64,
                CC_AST_TYPE_FLAGS_DECIMAL128,
                /* Properties and qualifiers */
                CC_AST_TYPE_FLAGS_SIGNED = 0x10,
                /* Uses children */
                CC_AST_TYPE_FLAGS_STRUCT = 0x20,
                CC_AST_TYPE_FLAGS_UNION,
                CC_AST_TYPE_FLAGS_ENUM,
                CC_AST_TYPE_FLAGS_FUNCTION,
                /* _BitInt() - This represents BitInt(1) */
                CC_AST_TYPE_FLAGS_BITINT,
            } flags;
            unsigned int qual_length[CHAR_BIT]; /* For static sized arrays [4][5][3] */
            unsigned char qual_v;
            unsigned char qual_c;
        } new_type;
        struct {
            cc_strview_t name;
            cc_ast_node_ref_t init;
        } new_var;
        struct {
            cc_ast_node_ref_t *childs;
            size_t n_childs;
        } block;
        struct {
            cc_ast_node_ref_t cond;
            cc_ast_node_ref_t then;
            cc_ast_node_ref_t else_;
        } if_expr;
        struct {
            cc_ast_node_ref_t child;
        } cast;
        struct {
            cc_ast_node_ref_t left;
            cc_ast_node_ref_t op[2];
        } binop;
        struct {
            cc_ast_node_ref_t left;
            cc_ast_node_ref_t op;
        } unop;
        long double literal_ld;
        unsigned long long literal_ull;
    } data;
    enum cc_ast_node_type {
        CC_AST_NODE_INVALID,
        CC_AST_NODE_NEW_VARIABLE,
        CC_AST_NODE_NEW_TYPE,
        CC_AST_NODE_REF_TYPE,
        CC_AST_NODE_REF_VAR,
        CC_AST_NODE_BLOCK,
        CC_AST_NODE_IF,
        CC_AST_NODE_CAST,
        CC_AST_NODE_LITERAL_FLOAT,
        CC_AST_NODE_LITERAL_INTEGER,
        /* Binary operations A = B <op> C */
#define CC_AST_NODE_BINOP (CC_AST_NODE_BINOP_ADD)
        CC_AST_NODE_BINOP_ADD = 0x40,
        CC_AST_NODE_BINOP_SUB,
        CC_AST_NODE_BINOP_MUL,
        CC_AST_NODE_BINOP_DIV,
        CC_AST_NODE_BINOP_REM,
        CC_AST_NODE_BINOP_BIT_AND,
        CC_AST_NODE_BINOP_BIT_XOR,
        CC_AST_NODE_BINOP_BIT_OR,
        CC_AST_NODE_BINOP_LSHIFT,
        CC_AST_NODE_BINOP_RSHIFT,
        CC_AST_NODE_BINOP_COND_EQ,
        CC_AST_NODE_BINOP_COND_NEQ,
        CC_AST_NODE_BINOP_COND_GT,
        CC_AST_NODE_BINOP_COND_LT,
        CC_AST_NODE_BINOP_COND_GTE,
        CC_AST_NODE_BINOP_COND_LTE,
        CC_AST_NODE_BINOP_COND_AND,
        CC_AST_NODE_BINOP_COND_OR,
        CC_AST_NODE_BINOP_COND_XOR,
        /* Unary operations A = <op> B */
#define CC_AST_NODE_UNOP (CC_AST_NODE_UNOP_BIT_NOT)
        CC_AST_NODE_UNOP_BIT_NOT = 0x80,
        CC_AST_NODE_UNOP_COND_NOT,
        CC_AST_NODE_UNOP_PLUS,
        CC_AST_NODE_UNOP_MINUS,
    } type;
} cc_ast_node_t;

#endif
