#ifndef LEXER_H
#define LEXER_H 1

#include "strview.h"

#define TOKEN_LIST \
    TOKEN_ELEM(ELLIPSIS, "...") \
    TOKEN_ELEM(ASSIGN_LSHIFT, "<<=") \
    TOKEN_ELEM(ASSIGN_RSHIFT, ">>=") \
    TOKEN_ELEM(DOUBLE_COLON, "::") \
    TOKEN_ELEM(INCREMENT, "++") \
    TOKEN_ELEM(DECREMENT, "--") \
    TOKEN_ELEM(COND_LTE, "<=") \
    TOKEN_ELEM(COND_GTE, ">=") \
    TOKEN_ELEM(COND_EQ, "==") \
    TOKEN_ELEM(COND_NEQ, "!=") \
    TOKEN_ELEM(COND_OR, "||") \
    TOKEN_ELEM(COND_AND, "&&") \
    TOKEN_ELEM(ASSIGN_ADD, "+=") \
    TOKEN_ELEM(ASSIGN_SUB, "-=") \
    TOKEN_ELEM(ASSIGN_DIV, "/=") \
    TOKEN_ELEM(ASSIGN_REM, "%=") \
    TOKEN_ELEM(ASSIGN_MUL, "*=") \
    TOKEN_ELEM(ASSIGN_AND, "&=") \
    TOKEN_ELEM(ASSIGN_XOR, "^=") \
    TOKEN_ELEM(ASSIGN_OR, "|=") \
    TOKEN_ELEM(LSHIFT, "<<") \
    TOKEN_ELEM(RSHIFT, ">>") \
    TOKEN_ELEM(ARROW, "->") \
    TOKEN_ELEM(COND_NOT, "!") \
    TOKEN_ELEM(NOT, "~") \
    TOKEN_ELEM(ADD, "+") \
    TOKEN_ELEM(SUB, "-") \
    TOKEN_ELEM(DIV, "/") \
    TOKEN_ELEM(REM, "%") \
    TOKEN_ELEM(ASTERISK, "*") \
    TOKEN_ELEM(AMPERSAND, "&") \
    TOKEN_ELEM(BIT_XOR, "^") \
    TOKEN_ELEM(BIT_OR, "|") \
    TOKEN_ELEM(QMARK, "?") \
    TOKEN_ELEM(COLON, ":") \
    TOKEN_ELEM(SEMI, ";") \
    TOKEN_ELEM(COMMA, ",") \
    TOKEN_ELEM(COND_LT, "<") \
    TOKEN_ELEM(COND_GT, ">") \
    TOKEN_ELEM(ASSIGN, "=") \
    TOKEN_ELEM(LBRACE, "{") \
    TOKEN_ELEM(RBRACE, "}") \
    TOKEN_ELEM(LPAREN, "(") \
    TOKEN_ELEM(RPAREN, ")") \
    TOKEN_ELEM(LBRACKET, "[") \
    TOKEN_ELEM(RBRACKET, "]") \
    TOKEN_ELEM(HASHTAG, "#") \
    TOKEN_ELEM(DOT, ".") \
    TOKEN_KW(_Static_assert) \
    TOKEN_KW(_Thread_local) \
    TOKEN_KW(typeof_unqual) \
    TOKEN_KW(static_assert) \
    TOKEN_KW(thread_local) \
    TOKEN_KW(_Decimal128) \
    TOKEN_KW(_Imaginary) \
    TOKEN_KW(_Decimal64) \
    TOKEN_KW(_Decimal32) \
    TOKEN_KW(_Noreturn) \
    TOKEN_KW(constexpr) \
    TOKEN_KW(_Generic) \
    TOKEN_KW(_Complex) \
    TOKEN_KW(_Alignof) \
    TOKEN_KW(_Alignas) \
    TOKEN_KW(volatile) \
    TOKEN_KW(unsigned) \
    TOKEN_KW(restrict) \
    TOKEN_KW(register) \
    TOKEN_KW(continue) \
    TOKEN_KW(_BitInt) \
    TOKEN_KW(_Atomic) \
    TOKEN_KW(typedef) \
    TOKEN_KW(nullptr) \
    TOKEN_KW(default) \
    TOKEN_KW(alignof) \
    TOKEN_KW(alignas) \
    TOKEN_KW(typeof) \
    TOKEN_KW(switch) \
    TOKEN_KW(struct) \
    TOKEN_KW(static) \
    TOKEN_KW(sizeof) \
    TOKEN_KW(signed) \
    TOKEN_KW(return) \
    TOKEN_KW(inline) \
    TOKEN_KW(extern) \
    TOKEN_KW(double) \
    TOKEN_KW(_Bool) \
    TOKEN_KW(while) \
    TOKEN_KW(union) \
    TOKEN_KW(short) \
    TOKEN_KW(float) \
    TOKEN_KW(false) \
    TOKEN_KW(const) \
    TOKEN_KW(break) \
    TOKEN_KW(void) \
    TOKEN_KW(true) \
    TOKEN_KW(long) \
    TOKEN_KW(goto) \
    TOKEN_KW(enum) \
    TOKEN_KW(else) \
    TOKEN_KW(char) \
    TOKEN_KW(case) \
    TOKEN_KW(bool) \
    TOKEN_KW(auto) \
    TOKEN_KW(int) \
    TOKEN_KW(for) \
    TOKEN_KW(if) \
    TOKEN_KW(do)

typedef struct cc_token {
    cc_strview_t data;
    enum cc_token_type {
        CC_TOK_INVALID = 0,
        CC_TOK_IDENT,
        CC_TOK_LITERAL,
#define TOKEN_KW(NAME) CC_TOK_##NAME,
#define TOKEN_ELEM(NAME, ...) CC_TOK_##NAME,
        TOKEN_LIST
#undef TOKEN_ELEM
#undef TOKEN_KW
    } type;
} cc_token_t;

typedef struct cc_state cc_state_t;
void cc_tokenize(cc_state_t *state);
void cc_dump_tokens(cc_state_t *state);

#endif
