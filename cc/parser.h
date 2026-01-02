#ifndef PARSER_H
#define PARSER_H 1

#include "ast.h"

typedef struct cc_state cc_state_t;
cc_ast_node_ref_t cc_parse_translation_unit(cc_state_t *state);

#endif
