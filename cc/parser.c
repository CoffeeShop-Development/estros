#include <stdbool.h>
#include <stdlib.h>
#include "ast.h"
#include "lexer.h"
#include "state.h"
#include "util.h"

static struct cc_token cc_peek_token(cc_state_t *state, size_t offset) {
    struct cc_token tok = {0};
    return state->cur_tok + offset < state->n_tokens ? state->tokens[state->cur_tok + offset] : tok;
}
static struct cc_token cc_next_token(cc_state_t *state) {
    struct cc_token tok = {0};
    ++state->cur_tok;
    return state->cur_tok < state->n_tokens ? state->tokens[state->cur_tok] : tok;
}

typedef struct {
    cc_ast_node_ref_t ref;
    bool is_new;
} cc_ast_new_block_result_t;
/* Pushes an AST block but only IF the previous block isn't empty */
static cc_ast_new_block_result_t cc_ast_push_block_coalesce(cc_state_t *state, cc_ast_node_ref_t outer_expr) {
    cc_ast_new_block_result_t res;
    cc_ast_node_t *node = &state->nodes[outer_expr];
    res.ref = outer_expr;
    res.is_new = node->type != CC_AST_NODE_BLOCK || node->data.block.n_childs > 0;
    if (res.is_new)
        res.ref = cc_ast_push_node(state, (cc_ast_node_t){ .type = CC_AST_NODE_BLOCK, });
    return res;
}
static void cc_ast_pop_block_coalesce(cc_state_t *state, cc_ast_new_block_result_t res) {
    if (res.is_new)
        cc_ast_pop_node(state, res.ref);
}

#define CC_AST_NIL ((cc_ast_node_ref_t)(0))

static bool cc_parse_expression(cc_state_t *state, cc_ast_node_ref_t expr, bool lock);
static bool cc_parse_unary_expression(cc_state_t *state, cc_ast_node_ref_t expr);

#define CC_PARSE_BINOP_DEFINE(NAME, NEXT, NODE_TYPE, TOK_TYPE) \
static bool NAME(cc_state_t *state, cc_ast_node_ref_t expr, bool lock) { \
    cc_ast_node_ref_t lhs_expr = cc_ast_push_node(state, (cc_ast_node_t){ .type = CC_AST_NODE_BLOCK, }); \
    cc_token_t tok; \
    if (NEXT(state, lhs_expr, false)) { \
        if ((tok = cc_peek_token(state, 0)).type == TOK_TYPE) { \
            cc_ast_node_ref_t rhs_expr = cc_ast_push_node(state, (cc_ast_node_t){ .type = CC_AST_NODE_BLOCK, }); \
            cc_ast_node_ref_t op_expr = cc_ast_push_node(state, (cc_ast_node_t){ \
                .type = NODE_TYPE, \
                .data.binop.lhs = lhs_expr, \
                .data.binop.rhs = rhs_expr, \
            }); \
            cc_next_token(state); \
            if (NEXT(state, rhs_expr, false)) { \
                cc_ast_push_children_to_block(state, expr, op_expr); \
                return true; \
            } \
            abort(); \
            cc_ast_pop_node(state, op_expr); \
            cc_ast_pop_node(state, rhs_expr); \
            return false; \
        } \
        cc_ast_push_children_to_block(state, expr, lhs_expr); \
        return true; \
    } else if (!lock) { \
        if (NAME(state, lhs_expr, true) && (tok = cc_peek_token(state, 0)).type == TOK_TYPE) { \
            cc_ast_node_ref_t rhs_expr = cc_ast_push_node(state, (cc_ast_node_t){ .type = CC_AST_NODE_BLOCK, }); \
            cc_ast_node_ref_t op_expr = cc_ast_push_node(state, (cc_ast_node_t){ \
                .type = NODE_TYPE, \
                .data.binop.lhs = lhs_expr, \
                .data.binop.rhs = rhs_expr, \
            }); \
            cc_next_token(state); \
            if (NEXT(state, rhs_expr, false)) { \
                cc_ast_push_children_to_block(state, expr, op_expr); \
                return true; \
            } \
            abort(); \
            cc_ast_pop_node(state, op_expr); \
            cc_ast_pop_node(state, rhs_expr); \
            return false; \
        } \
    } \
    cc_ast_pop_node(state, lhs_expr); \
    return false; \
}

static bool cc_parse_cast_expression(cc_state_t *state, cc_ast_node_ref_t expr, bool lock) {
    if (cc_parse_unary_expression(state, expr))
        return true;
    return false;
}

CC_PARSE_BINOP_DEFINE(cc_parse_multiplicative_expression_2, cc_parse_cast_expression,
    CC_AST_NODE_BINOP_MUL, CC_TOK_ASTERISK);
CC_PARSE_BINOP_DEFINE(cc_parse_multiplicative_expression_1, cc_parse_multiplicative_expression_2,
    CC_AST_NODE_BINOP_DIV, CC_TOK_DIV);
CC_PARSE_BINOP_DEFINE(cc_parse_additive_expression_2, cc_parse_multiplicative_expression_1,
    CC_AST_NODE_BINOP_SUB, CC_TOK_SUB);
CC_PARSE_BINOP_DEFINE(cc_parse_additive_expression_1, cc_parse_additive_expression_2,
    CC_AST_NODE_BINOP_ADD, CC_TOK_ADD);
CC_PARSE_BINOP_DEFINE(cc_parse_shift_expression_2, cc_parse_additive_expression_1,
    CC_AST_NODE_BINOP_LSHIFT, CC_TOK_LSHIFT);
CC_PARSE_BINOP_DEFINE(cc_parse_shift_expression_1, cc_parse_shift_expression_2,
    CC_AST_NODE_BINOP_RSHIFT, CC_TOK_RSHIFT);
CC_PARSE_BINOP_DEFINE(cc_parse_relational_expression_3, cc_parse_shift_expression_1,
    CC_AST_NODE_BINOP_COND_LTE, CC_TOK_COND_LTE);
CC_PARSE_BINOP_DEFINE(cc_parse_relational_expression_2, cc_parse_relational_expression_3,
    CC_AST_NODE_BINOP_COND_GTE, CC_TOK_COND_GTE);
CC_PARSE_BINOP_DEFINE(cc_parse_relational_expression_1, cc_parse_relational_expression_2,
    CC_AST_NODE_BINOP_COND_LT, CC_TOK_COND_LT);
CC_PARSE_BINOP_DEFINE(cc_parse_equality_expression_2, cc_parse_relational_expression_1,
    CC_AST_NODE_BINOP_COND_GT, CC_TOK_COND_GT);
CC_PARSE_BINOP_DEFINE(cc_parse_equality_expression_1, cc_parse_equality_expression_2,
    CC_AST_NODE_BINOP_COND_NEQ, CC_TOK_COND_NEQ);
CC_PARSE_BINOP_DEFINE(cc_parse_and_expression, cc_parse_equality_expression_1,
    CC_AST_NODE_BINOP_BIT_AND, CC_TOK_AMPERSAND);
CC_PARSE_BINOP_DEFINE(cc_parse_exclusive_or_expression, cc_parse_and_expression,
    CC_AST_NODE_BINOP_BIT_XOR, CC_TOK_BIT_XOR);
CC_PARSE_BINOP_DEFINE(cc_parse_inclusive_or_expression, cc_parse_exclusive_or_expression,
    CC_AST_NODE_BINOP_BIT_OR, CC_TOK_BIT_OR);
CC_PARSE_BINOP_DEFINE(cc_parse_logcal_and_expression, cc_parse_inclusive_or_expression,
    CC_AST_NODE_BINOP_COND_AND, CC_TOK_COND_AND);
CC_PARSE_BINOP_DEFINE(cc_parse_logcal_or_expression, cc_parse_logcal_and_expression,
    CC_AST_NODE_BINOP_COND_OR, CC_TOK_COND_OR);

static bool cc_parse_conditional_expression(cc_state_t *state, cc_ast_node_ref_t expr) {
    cc_ast_node_ref_t cond_expr = cc_ast_push_node(state, (cc_ast_node_t){
        .type = CC_AST_NODE_BLOCK,
    });
    if (cc_parse_logcal_or_expression(state, cond_expr, false)) {
        cc_token_t tok = cc_peek_token(state, 0);
        assert(cond_expr != expr);
        if (tok.type == CC_TOK_QMARK) {
            cc_ast_node_ref_t then_expr = cc_ast_push_node(state, (cc_ast_node_t){
                .type = CC_AST_NODE_BLOCK,
            });
            cc_ast_node_ref_t else_expr = cc_ast_push_node(state, (cc_ast_node_t){
                .type = CC_AST_NODE_BLOCK,
            });
            cc_ast_node_ref_t if_expr = cc_ast_push_node(state, (cc_ast_node_t){
                .type = CC_AST_NODE_IF,
                .data.if_expr.cond = cond_expr,
                .data.if_expr.then = then_expr,
                .data.if_expr.else_ = else_expr,
            });
            cc_next_token(state);
            if (cc_parse_expression(state, then_expr, false)) {
                if ((tok = cc_peek_token(state, 0)).type == CC_TOK_COLON) {
                    cc_next_token(state);
                    if (cc_parse_conditional_expression(state, else_expr)) {
                        cc_ast_push_children_to_block(state, expr, if_expr);
                        return true;
                    }
                    abort();
                }
                abort();
            }
            abort();
            cc_ast_pop_node(state, if_expr);
            cc_ast_pop_node(state, else_expr);
            cc_ast_pop_node(state, then_expr);
        }
        cc_ast_push_children_to_block(state, expr, cond_expr);
        return true;
    }
    cc_ast_pop_node(state, cond_expr);
    return false;
}
static bool cc_parse_primary_expression(cc_state_t *state, cc_ast_node_ref_t expr) {
    cc_token_t tok = cc_peek_token(state, 0);
    if (tok.type == CC_TOK_IDENT) {
        cc_ast_node_ref_t primary = cc_ast_push_node(state, (cc_ast_node_t){
            .type = CC_AST_NODE_REF_VAR,
            .data.var_name = tok.data
        });
        cc_next_token(state);
        cc_ast_push_children_to_block(state, expr, primary);
        return true;
    } else if (tok.type == CC_TOK_LITERAL) {
        cc_ast_node_ref_t primary = cc_ast_push_node(state, (cc_ast_node_t){
            .type = CC_AST_NODE_LITERAL_INTEGER,
            .data.literal_ull = atoll(cc_strview_get(state, tok.data))
        });
        cc_next_token(state);
        cc_ast_push_children_to_block(state, expr, primary);
        return true;
    }
    return false;
}
static bool cc_parse_postfix_expression(cc_state_t *state, cc_ast_node_ref_t expr) {
    cc_ast_new_block_result_t inner = cc_ast_push_block_coalesce(state, expr);
    if (cc_parse_primary_expression(state, inner.ref))
        return true;
    cc_ast_pop_block_coalesce(state, inner);
    return false;
}
static bool cc_parse_unary_expression(cc_state_t *state, cc_ast_node_ref_t expr) {
    cc_ast_new_block_result_t inner = cc_ast_push_block_coalesce(state, expr);
    if (cc_parse_postfix_expression(state, inner.ref))
        return true;
    cc_ast_pop_block_coalesce(state, inner);
    return false;
}
static bool cc_parse_assignment_operator(cc_state_t *state, cc_ast_node_ref_t expr) {
    cc_token_t tok = cc_peek_token(state, 0);
    cc_ast_node_t *node = &state->nodes[expr];
    assert(node->type >= CC_AST_NODE_BINOP);
    if (tok.type == CC_TOK_ASSIGN) {
        cc_next_token(state);
        node->type = CC_AST_NODE_BINOP_ASSIGN;
        return true;
    } else if (tok.type == CC_TOK_ASSIGN_ADD) {
        abort();
    } else if (tok.type == CC_TOK_ASSIGN_SUB) {
        abort();
    } else if (tok.type == CC_TOK_ASSIGN_MUL) {
        abort();
    } else if (tok.type == CC_TOK_ASSIGN_DIV) {
        abort();
    } else if (tok.type == CC_TOK_ASSIGN_REM) {
        abort();
    } else if (tok.type == CC_TOK_ASSIGN_LSHIFT) {
        abort();
    } else if (tok.type == CC_TOK_ASSIGN_RSHIFT) {
        abort();
    } else if (tok.type == CC_TOK_ASSIGN_AND) {
        abort();
    } else if (tok.type == CC_TOK_ASSIGN_OR) {
        abort();
    } else if (tok.type == CC_TOK_ASSIGN_XOR) {
        abort();
    }
    return false;
}
static bool cc_parse_assignment_expression(cc_state_t *state, cc_ast_node_ref_t expr) {
    if (cc_parse_conditional_expression(state, expr)) {
        return true;
    } else {
        cc_ast_node_ref_t lhs_expr = cc_ast_push_node(state, (cc_ast_node_t){
            .type = CC_AST_NODE_BLOCK,
        });
        cc_ast_node_ref_t rhs_expr = cc_ast_push_node(state, (cc_ast_node_t){
            .type = CC_AST_NODE_BLOCK,
        });
        cc_ast_node_ref_t assign_expr = cc_ast_push_node(state, (cc_ast_node_t){
            .type = CC_AST_NODE_BINOP_ASSIGN,
            .data.binop.lhs = lhs_expr,
            .data.binop.rhs = lhs_expr,
        });
        if (cc_parse_unary_expression(state, lhs_expr)) {
            if (cc_parse_assignment_operator(state, assign_expr)) {
                cc_parse_assignment_expression(state, rhs_expr);
                cc_ast_push_children_to_block(state, expr, assign_expr);
                return true;
            }
            abort();
        }
        cc_ast_pop_node(state, assign_expr);
        cc_ast_pop_node(state, rhs_expr);
        cc_ast_pop_node(state, lhs_expr);
    }
    return false;
}
static bool cc_parse_expression(cc_state_t *state, cc_ast_node_ref_t expr, bool lock) {
    if (cc_parse_assignment_expression(state, expr)) {
        return true;
    } else {
        if (!lock && cc_parse_expression(state, expr, true)) {
            abort();
        }
    }
    return false;
}
static bool cc_parse_jump_statment(cc_state_t *state, cc_ast_node_ref_t expr) {
    cc_token_t tok;
    if ((tok = cc_peek_token(state, 0)).type == CC_TOK_return) {
        cc_ast_node_ref_t inner_ret_expr = cc_ast_push_node(state, (cc_ast_node_t){
            .type = CC_AST_NODE_BLOCK,
        });
        cc_ast_node_ref_t ret_expr = cc_ast_push_node(state, (cc_ast_node_t){
            .type = CC_AST_NODE_RETURN,
            .data.retval = inner_ret_expr
        });
        cc_next_token(state);
        if ((tok = cc_peek_token(state, 0)).type == CC_TOK_SEMI) {
            cc_next_token(state);
            cc_ast_push_children_to_block(state, expr, ret_expr);
            return true;
        } else if (cc_parse_expression(state, inner_ret_expr, false)) {
            if ((tok = cc_peek_token(state, 0)).type == CC_TOK_SEMI) {
                cc_next_token(state);
                cc_ast_push_children_to_block(state, expr, ret_expr);
                return true;
            }
            abort();
        }
        abort();
        cc_ast_pop_node(state, ret_expr);
        cc_ast_pop_node(state, inner_ret_expr);
    }
    return false;
}
static bool cc_parse_statment(cc_state_t *state, cc_ast_node_ref_t expr) {
    return cc_parse_jump_statment(state, expr);
}
static bool cc_parse_statment_list(cc_state_t *state, cc_ast_node_ref_t expr) {
    cc_ast_new_block_result_t inner = cc_ast_push_block_coalesce(state, expr);
    if (cc_parse_statment(state, inner.ref)) {
        while (cc_parse_statment(state, inner.ref));
        return true;
    }
    cc_ast_pop_block_coalesce(state, inner);
    return false;
}
static bool cc_parse_declaration_list(cc_state_t *state, cc_ast_node_ref_t expr) {
    return false;
}
static bool cc_parse_compound_statement(cc_state_t *state, cc_ast_node_ref_t expr) {
    cc_token_t tok;
    if ((tok = cc_peek_token(state, 0)).type == CC_TOK_LBRACE) {
        cc_next_token(state);
        if ((tok = cc_peek_token(state, 0)).type == CC_TOK_RBRACE) {
            cc_next_token(state);
            return true;
        }
        if (cc_parse_statment_list(state, expr)) {

        } else if (cc_parse_declaration_list(state, expr)) {
            cc_parse_statment_list(state, expr);
        } else {
            abort();
        }
        tok = cc_peek_token(state, 0);
        if (tok.type == CC_TOK_RBRACE) {
            cc_next_token(state);
            return true;
        }
        abort();
    }
    return false;
}

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
            if ((tok = cc_peek_token(state, 0)).type == CC_TOK_RPAREN) {
                cc_next_token(state);
                return true;
            } else {
                abort();
            }
        }
        return true;
    } else if (tok.type == CC_TOK_LPAREN) {
        cc_next_token(state);
        if (cc_parse_declarator(state, decl)) {
            if ((tok = cc_peek_token(state, 0)).type == CC_TOK_RPAREN) {
                cc_next_token(state);
                return true;
            } else {
                abort();
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
    cc_ast_node_t *node = &state->nodes[decl];
    assert(node->type == CC_AST_NODE_NEW_VAR);
    cc_parse_declaration_specifiers(state, decl);
    if (cc_parse_declarator(state, decl))
        return cc_parse_compound_statement(state, node->data.new_var.init);
    return false;
}

static cc_ast_node_ref_t cc_parse_external_declaration(cc_state_t *state) {
    cc_ast_node_ref_t type_def = cc_ast_push_node(state, (cc_ast_node_t){
        .type = CC_AST_NODE_NEW_TYPE,
    });
    cc_ast_node_ref_t decl_init = cc_ast_push_node(state, (cc_ast_node_t){
        .type = CC_AST_NODE_BLOCK,
    });
    cc_ast_node_ref_t decl = cc_ast_push_node(state, (cc_ast_node_t){
        .type = CC_AST_NODE_NEW_VAR,
        .data.new_var.type_def = type_def,
        .data.new_var.init = decl_init,
    });
    if (cc_parse_function_definition(state, decl))
        return decl;
    /* Must be popped in order */
    cc_ast_pop_node(state, decl);
    cc_ast_pop_node(state, decl_init);
    cc_ast_pop_node(state, type_def);
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
