#include <stdbool.h>
#include <stdlib.h>
#include "ast.h"
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

#define CC_AST_NIL ((cc_ast_node_ref_t)(0))

static bool cc_parse_declarator(cc_state_t *state, cc_ast_node_ref_t decl);
static bool cc_parse_declaration_specifiers(cc_state_t *state, cc_ast_node_ref_t decl);

static bool cc_parse_abstract_declarator(cc_state_t *state, cc_ast_node_ref_t decl) {
    return false;
}
static bool cc_parse_parameter_declaration(cc_state_t *state, cc_ast_node_ref_t decl) {
    cc_ast_node_ref_t param_type_def = cc_ast_push_node(state, (cc_ast_node_t){
        .type = CC_AST_NODE_NEW_TYPE,
    });
    cc_ast_node_ref_t param_decl = cc_ast_push_node(state, (cc_ast_node_t){
        .type = CC_AST_NODE_NEW_VAR,
        .data.new_var.type_def = param_type_def
    });
    if (cc_parse_declaration_specifiers(state, param_decl)) {
        cc_ast_node_t *node = &state->nodes[decl];
        assert(node->type == CC_AST_NODE_NEW_VAR);
        if (cc_parse_declarator(state, param_decl)) {
            cc_ast_push_children_to_type(state, node->data.new_var.type_def, param_decl);
            return true;
        } else if (cc_parse_abstract_declarator(state, param_decl)) {
            abort();
        }
        abort();
    }
    /* Must be popped in order */
    cc_ast_pop_node(state, param_type_def);
    cc_ast_pop_node(state, param_decl);
    return false;
}
static bool cc_parse_parameter_list(cc_state_t *state, cc_ast_node_ref_t decl) {
    while (cc_parse_parameter_declaration(state, decl)) {
        cc_token_t tok = cc_peek_token(state, 0);
        if (tok.type == CC_TOK_COMMA) {
            cc_next_token(state);
            continue;
        }
        break;
    }
    return false;
}
static bool cc_parse_parameter_type_list(cc_state_t *state, cc_ast_node_ref_t decl) {
    if (cc_parse_parameter_list(state, decl)) {
        cc_token_t tok = cc_peek_token(state, 0);
        if (tok.type == CC_TOK_COMMA) {
            cc_next_token(state);
            tok = cc_peek_token(state, 0);
            if (tok.type == CC_TOK_ELLIPSIS) {
                return true;
            }
            abort();
            return false;
        }
        return true;
    }
    return false;
}
static bool cc_parse_pointer(cc_state_t *state, cc_ast_node_ref_t decl) {
    return false;
}
static bool cc_parse_direct_declarator(cc_state_t *state, cc_ast_node_ref_t decl) {
    cc_token_t tok = cc_peek_token(state, 0);
    cc_ast_node_t *node = &state->nodes[decl];
    assert(node->type == CC_AST_NODE_NEW_VAR);
    if (tok.type == CC_TOK_IDENT) {
        cc_next_token(state);
        node->data.new_var.name = tok.data;

        tok = cc_peek_token(state, 0);
        if (tok.type == CC_TOK_LPAREN) {
            cc_next_token(state);
            cc_parse_parameter_type_list(state, decl);

        }
        return true;
    } else if (tok.type == CC_TOK_LPAREN) {
        cc_next_token(state);
        if (cc_parse_declarator(state, decl)) {
            if ((tok = cc_peek_token(state, 0)).type == CC_TOK_RPAREN) {
                cc_next_token(state);
                return true;
            }
        }
        abort();
        return false;
    }
    return false;
}
static bool cc_parse_declarator(cc_state_t *state, cc_ast_node_ref_t decl) {
    cc_parse_pointer(state, decl);
    return cc_parse_direct_declarator(state, decl);
}
static cc_ast_node_ref_t cc_parse_declaration_list(cc_state_t *state) {
    return CC_AST_NIL;
}
static cc_ast_node_ref_t cc_parse_compound_statement(cc_state_t *state) {
    return CC_AST_NIL;
}
static bool cc_parse_storage_class_specifier(cc_state_t *state, cc_ast_node_ref_t decl) {
    cc_token_t tok = cc_peek_token(state, 0);
    cc_ast_node_t *node = &state->nodes[decl];
    assert(node->type == CC_AST_NODE_NEW_VAR);
    if (tok.type == CC_TOK_typedef) {
        cc_next_token(state);
        node->data.new_var.flags |= CC_AST_VAR_FLAGS_TYPEDEF;
        return true;
    } else if (tok.type == CC_TOK_thread_local || tok.type == CC_TOK__Thread_local) {
        cc_next_token(state);
        node->data.new_var.flags |= CC_AST_VAR_FLAGS_THREAD_LOCAL;
        return true;
    } else if (tok.type == CC_TOK_extern) {
        cc_next_token(state);
        node->data.new_var.flags |= CC_AST_VAR_FLAGS_EXTERN;
        return true;
    } else if (tok.type == CC_TOK_static) {
        cc_next_token(state);
        node->data.new_var.flags |= CC_AST_VAR_FLAGS_STATIC;
        return true;
    } else if (tok.type == CC_TOK_register || tok.type == CC_TOK_auto) {
        cc_next_token(state);
        return true;
    }
    return false;
}
static bool cc_parse_type_specifier(cc_state_t *state, cc_ast_node_ref_t type_def) {
    cc_token_t tok = cc_peek_token(state, 0);
    cc_ast_node_t *node = &state->nodes[type_def];
    assert(node->type == CC_AST_NODE_NEW_TYPE);
    if (tok.type == CC_TOK_void) {
        cc_next_token(state);
        node->data.new_type.flags |= CC_AST_TYPE_FLAGS_VOID;
        return true;
    } else if (tok.type == CC_TOK_char) {
        cc_next_token(state);
        node->data.new_type.flags |= CC_AST_TYPE_FLAGS_CHAR;
        return true;
    } else if (tok.type == CC_TOK_short) {
        cc_next_token(state);
        node->data.new_type.flags |= CC_AST_TYPE_FLAGS_SHORT;
        return true;
    } else if (tok.type == CC_TOK_int) {
        cc_next_token(state);
        node->data.new_type.flags |= CC_AST_TYPE_FLAGS_INT;
        return true;
    } else if (tok.type == CC_TOK_long) {
        cc_next_token(state);
        if ((node->data.new_var.flags & ~CC_AST_TYPE_FLAGS_SIGNED) == CC_AST_TYPE_FLAGS_LONG)
            node->data.new_type.flags |= CC_AST_TYPE_FLAGS_LONG_LONG;
        else
            node->data.new_type.flags |= CC_AST_TYPE_FLAGS_LONG;
        return true;
    } else if (tok.type == CC_TOK_float) {
        cc_next_token(state);
        node->data.new_type.flags |= CC_AST_TYPE_FLAGS_FLOAT;
        return true;
    } else if (tok.type == CC_TOK_double) {
        cc_next_token(state);
        node->data.new_type.flags |= CC_AST_TYPE_FLAGS_DOUBLE;
        return true;
    } else if (tok.type == CC_TOK_bool || tok.type == CC_TOK__Bool) {
        cc_next_token(state);
        node->data.new_type.flags |= CC_AST_TYPE_FLAGS_BOOL;
        return true;
    }
    return false;
}
static bool cc_parse_type_qualifier(cc_state_t *state) {
    return false;
}
static bool cc_parse_declaration_specifiers(cc_state_t *state, cc_ast_node_ref_t decl) {
    cc_ast_node_t *node = &state->nodes[decl];
    assert(node->type == CC_AST_NODE_NEW_VAR);
    if (cc_parse_storage_class_specifier(state, decl)
    || cc_parse_type_specifier(state, node->data.new_var.type_def)
    || cc_parse_type_qualifier(state)) {
        cc_parse_declaration_specifiers(state, decl);
        return true;
    }
    return false;
}

static bool cc_parse_function_definition(cc_state_t *state, cc_ast_node_ref_t decl) {
    cc_parse_declaration_specifiers(state, decl);
    if (cc_parse_declarator(state, decl)) {
        return true;
    }
    return false;
}

static cc_ast_node_ref_t cc_parse_external_declaration(cc_state_t *state) {
    cc_ast_node_ref_t type_def = cc_ast_push_node(state, (cc_ast_node_t){
        .type = CC_AST_NODE_NEW_TYPE,
    });
    cc_ast_node_ref_t decl = cc_ast_push_node(state, (cc_ast_node_t){
        .type = CC_AST_NODE_NEW_VAR,
        .data.new_var.type_def = type_def
    });
    if (cc_parse_function_definition(state, decl))
        return decl;
    /* Must be popped in order */
    cc_ast_pop_node(state, type_def);
    cc_ast_pop_node(state, decl);
    return CC_AST_NIL;
}

void cc_parse_translation_unit(cc_state_t *state) {
    cc_ast_node_ref_t empty_node = cc_ast_push_node(state, (cc_ast_node_t) {0});
    cc_ast_node_ref_t block = cc_ast_push_node(state, (cc_ast_node_t){
        .type = CC_AST_NODE_BLOCK,
        .data.block.childs = NULL,
        .data.block.n_childs = 0
    });
    cc_ast_node_ref_t node;
    while ((node = cc_parse_external_declaration(state)) != CC_AST_NIL)
        cc_ast_push_children_to_block(state, block, node);
    cc_ast_dump(state, block);
}
