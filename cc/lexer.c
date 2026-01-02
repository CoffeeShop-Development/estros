#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "state.h"

static bool cc_is_start_identifier(int c) {
    return isalpha(c) || c == '_';
}

static bool cc_is_identifier(int c) {
    return isalpha(c) || c == '_' || isdigit(c);
}

static void cc_push_token(cc_state_t *state, struct cc_token tok) {
    state->tokens[state->n_tokens++] = tok;
}

static size_t cc_utf8_len(int c) {
    return (c >= 0x00 && c <= 0xbf) ? 1
        : (c >= 0xc0 && c <= 0xdf) ? 2
        : (c >= 0xe0 && c <= 0xef) ? 3
        : 4;
}

void cc_tokenize(cc_state_t *state) {
    char const *start = (char const *)state->data, *end = (char const *)state->data + state->n_data;
    for (char const *p = start; p < end; ) {
        while (isspace(*p) && p < end) p += cc_utf8_len(*p);
        if (*p != '\0' && p < end) {
            char const *const p_start = p;
            struct cc_token tok = {0};
            tok.data.pos = (size_t)(p - start);
            if (0) { /* Chain of else if below :) */ }
#define TOKEN_KW(NAME) \
    else if (p + strlen(#NAME) < end && strncmp(p, #NAME, strlen(#NAME)) == 0 \
    /* Check if this is part of a greater identifier (ex. int_thing) or whatever */ \
    && ((p + strlen(#NAME) + 1 >= end) || !cc_is_identifier(p[strlen(#NAME)]))) { \
        tok.type = CC_TOK_##NAME; \
        p += (tok.data.len = strlen(#NAME)); \
    }
#define TOKEN_ELEM(CNAME, STRING) \
    else if (p + strlen(STRING) < end && strncmp(p, STRING, strlen(STRING)) == 0) { \
        tok.type = CC_TOK_##CNAME; \
        p += (tok.data.len = strlen(STRING)); \
    }
            TOKEN_LIST
#undef TOKEN_ELEM
#undef TOKEN_KW
            else if (isdigit(*p)) {
                tok.type = CC_TOK_LITERAL;
                do p += cc_utf8_len(*p); while (isnumber(*p) && p < end);
                tok.data.len = (size_t)(p - p_start);
            } else {
                tok.type = CC_TOK_IDENT;
                do p += cc_utf8_len(*p); while (!ispunct(*p) && !isspace(*p) && p < end);
                tok.data.len = (size_t)(p - p_start);
            }
            cc_push_token(state, tok);
        }
    }
}

static const char *cc_token_get_type_name(enum cc_token_type t) {
    switch (t) {
#define TOKEN_KW(NAME) case CC_TOK_##NAME: return #NAME;
#define TOKEN_ELEM(CNAME, STRING) case CC_TOK_##CNAME: return STRING;
    TOKEN_LIST
#undef TOKEN_ELEM
#undef TOKEN_KW
    case CC_TOK_LITERAL: return "literal";
    case CC_TOK_IDENT: return "ident";
    default: return "invalid";
    }
}

static void cc_print_token(cc_state_t *state, struct cc_token tok) {
    printf("%s", cc_token_get_type_name(tok.type));
}

void cc_dump_tokens(cc_state_t *state) {
    for (size_t i = 0; i < state->n_tokens; ++i) {
        cc_print_token(state, state->tokens[i]);
        printf(", ");
    }
    printf("\n");
}
