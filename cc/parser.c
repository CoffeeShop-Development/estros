#include <stdbool.h>
#include "state.h"

static struct cc_token cc_peek_token(cc_state_t *state, size_t offset) {
    struct cc_token tok = {0};
    return state->cur_tok + offset < state->n_tokens ? state->tokens[state->cur_tok + offset] : tok;
}
static struct cc_token cc_next_token(cc_state_t *state) {
    struct cc_token tok = {0};
    ++state->cur_tok;
    return state->cur_tok < state->n_tokens ? state->tokens[state->cur_tok] : tok;
}

static bool cc_parse_function_definition(cc_state_t *state) {
    return false;
}
static bool cc_parse_init_declarator_list(cc_state_t *state) {
    return false;
}
static bool cc_parse_declaration_specifiers(cc_state_t *state) {
    return false;
}
static bool cc_parse_declaration(cc_state_t *state) {
    if (cc_parse_declaration_specifiers(state)) {
        if (cc_parse_init_declarator_list(state)) {

        }
        
    }
    return false;
}
static bool cc_parse_external_declaration(cc_state_t *state) {
    return cc_parse_function_definition(state)
        || cc_parse_declaration(state);
}
bool cc_parse_translation_unit(cc_state_t *state) {
    bool ret;
    while ((ret = cc_parse_external_declaration(state)))
        ;
    return ret;
}
