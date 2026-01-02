#include "state.h"
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

const char *cc_ast_get_name(enum cc_ast_node_type t) {
    switch (t) {
    case CC_AST_NODE_INVALID: return "invalid";
    case CC_AST_NODE_NEW_VAR: return "new-var";
    case CC_AST_NODE_NEW_TYPE: return "new-type";
    case CC_AST_NODE_REF_TYPE: return "ref-type";
    case CC_AST_NODE_REF_VAR: return "ref-var";
    case CC_AST_NODE_BLOCK: return "block";
    case CC_AST_NODE_IF: return "if";
    case CC_AST_NODE_CAST: return "cast";
    case CC_AST_NODE_RETURN: return "return";
    case CC_AST_NODE_LITERAL_FLOAT: return "float";
    case CC_AST_NODE_LITERAL_INTEGER: return "integer";
    case CC_AST_NODE_BINOP_ASSIGN: return "assign";
    case CC_AST_NODE_BINOP_ADD: return "add";
    case CC_AST_NODE_BINOP_SUB: return "sub";
    case CC_AST_NODE_BINOP_MUL: return "mul";
    case CC_AST_NODE_BINOP_DIV: return "div";
    case CC_AST_NODE_BINOP_REM: return "rem";
    case CC_AST_NODE_BINOP_BIT_AND: return "bit-and";
    case CC_AST_NODE_BINOP_BIT_XOR: return "bit-xor";
    case CC_AST_NODE_BINOP_BIT_OR: return "bit-or";
    case CC_AST_NODE_BINOP_LSHIFT: return "lshift";
    case CC_AST_NODE_BINOP_RSHIFT: return "rshift";
    case CC_AST_NODE_BINOP_COND_EQ: return "cond-eq";
    case CC_AST_NODE_BINOP_COND_NEQ: return "cond-neq";
    case CC_AST_NODE_BINOP_COND_GT: return "cond-gt";
    case CC_AST_NODE_BINOP_COND_LT: return "cond-lt";
    case CC_AST_NODE_BINOP_COND_GTE: return "cond-gte";
    case CC_AST_NODE_BINOP_COND_LTE: return "cond-lte";
    case CC_AST_NODE_BINOP_COND_AND: return "cond-and";
    case CC_AST_NODE_BINOP_COND_OR: return "cond-or";
    case CC_AST_NODE_BINOP_COND_XOR: return "cond-xor";
    case CC_AST_NODE_UNOP_BIT_NOT: return "bit-not";
    case CC_AST_NODE_UNOP_COND_NOT: return "cond-not";
    case CC_AST_NODE_UNOP_PLUS: return "plus";
    case CC_AST_NODE_UNOP_MINUS: return "minus";
    }
}

bool cc_ast_type_is_binop(enum cc_ast_node_type t) {
    return t >= CC_AST_NODE_BINOP && t < CC_AST_NODE_UNOP;
}

void cc_ast_dump(cc_state_t *state, cc_ast_node_ref_t ref) {
    cc_ast_node_t const *node = state->nodes + ref;
    if (node->type == CC_AST_NODE_INVALID)
        return (void)printf("nil");

    printf("(%u:%s", (unsigned)ref, cc_ast_get_name(node->type));
    switch (node->type) {
    case CC_AST_NODE_REF_VAR:
        if (node->data.var_name.pos) printf(" %s", cc_strview_get(state, node->data.var_name));
        break;
    case CC_AST_NODE_REF_TYPE:
        if (node->data.type_name.pos) printf(" %s", cc_strview_get(state, node->data.type_name));
        break;
    case CC_AST_NODE_NEW_TYPE:
        if (node->data.new_type.name.pos) printf(" %s", cc_strview_get(state, node->data.new_type.name));
        if (node->data.new_type.n_childs > 0) {
            for (size_t i = 0; i < node->data.new_type.n_childs; ++i) {
                printf("\n");
                cc_ast_dump(state, node->data.new_type.childs[i]);
            }
        }
        break;
    case CC_AST_NODE_NEW_VAR:
        printf(" %s ", cc_strview_get(state, node->data.new_var.name));
        cc_ast_dump(state, node->data.new_var.type_def);
        printf("(");
        cc_ast_dump(state, node->data.new_var.init);
        break;
    case CC_AST_NODE_RETURN:
        printf(" ");
        cc_ast_dump(state, node->data.retval);
        break;
    case CC_AST_NODE_IF:
        printf(" cond=");
        cc_ast_dump(state, node->data.if_expr.cond);
        printf(" then=");
        cc_ast_dump(state, node->data.if_expr.then);
        printf(" else=");
        cc_ast_dump(state, node->data.if_expr.else_);
        break;
    case CC_AST_NODE_BLOCK:
        printf("%zu ", node->data.block.n_childs);
        for (size_t i = 0; i < node->data.block.n_childs; ++i)
            cc_ast_dump(state, node->data.block.childs[i]);
        break;
    default:
        if (cc_ast_type_is_binop(node->type)) {
            cc_ast_dump(state, node->data.binop.lhs);
            cc_ast_dump(state, node->data.binop.rhs);
        }
        break;
    }
    printf(")");
}

cc_ast_node_ref_t cc_ast_push_node(cc_state_t *state, cc_ast_node_t node) {
    cc_ast_node_ref_t ref = state->n_nodes;
    state->nodes[state->n_nodes++] = node;
    return ref;
}

void cc_ast_pop_node(cc_state_t *state, cc_ast_node_ref_t ref) {
    assert(ref == state->n_nodes - 1);
    state->n_nodes = ref;
}

void cc_ast_push_children_to_block(cc_state_t *state, cc_ast_node_ref_t n, cc_ast_node_ref_t c) {
    cc_ast_node_t *bp = &state->nodes[n];
    assert(bp->type == CC_AST_NODE_BLOCK);
    assert(n != c);
    ++bp->data.block.n_childs;
    bp->data.block.childs = realloc(bp->data.block.childs, bp->data.block.n_childs * sizeof(cc_ast_node_ref_t));
    bp->data.block.childs[bp->data.block.n_childs - 1] = c;
}

void cc_ast_push_children_to_type(cc_state_t *state, cc_ast_node_ref_t n, cc_ast_node_ref_t c) {
    cc_ast_node_t *bp = &state->nodes[n];
    assert(bp->type == CC_AST_NODE_NEW_TYPE);
    assert(n != c);
    ++bp->data.new_type.n_childs;
    bp->data.new_type.childs = realloc(bp->data.new_type.childs, bp->data.new_type.n_childs * sizeof(cc_ast_node_ref_t));
    bp->data.new_type.childs[bp->data.new_type.n_childs - 1] = c;
}

void cc_ast_push_children_to_block_coalesce(cc_state_t *state,  cc_ast_node_ref_t n, cc_ast_node_ref_t c) {
    cc_ast_node_t *bp = &state->nodes[n];
    assert(bp->type == CC_AST_NODE_BLOCK);
    if (bp->data.block.n_childs == 0) {
        /* TODO: better management? */
        *bp = state->nodes[c];
    } else {
        cc_ast_push_children_to_block(state, n, c);
    }
}

cc_ast_node_ref_t cc_ast_copy_node(cc_state_t *state, cc_ast_node_ref_t n) {
    assert(n > 0 && n < state->n_nodes);
    return cc_ast_push_node(state, state->nodes[n]);
}

void cc_ast_coalesce(cc_state_t *state, cc_ast_node_ref_t *pref) {
    cc_ast_node_t *node = state->nodes + *pref;
    switch (node->type) {
    case CC_AST_NODE_BLOCK:
        for (size_t i = 0; i < node->data.block.n_childs; ++i)
            cc_ast_coalesce(state, &node->data.block.childs[i]);
        /* TODO: fix this bullshit */
        if (node->data.block.n_childs == 1)
            *pref = node->data.block.childs[0];
        break;
    case CC_AST_NODE_NEW_TYPE:
        cc_ast_coalesce(state, &node->data.new_type.return_type);
        for (size_t i = 0; i < node->data.new_type.n_childs; ++i)
            cc_ast_coalesce(state, &node->data.new_type.childs[i]);
        break;
    case CC_AST_NODE_NEW_VAR:
        cc_ast_coalesce(state, &node->data.new_var.type_def);
        cc_ast_coalesce(state, &node->data.new_var.init);
        break;
    case CC_AST_NODE_IF:
        cc_ast_coalesce(state, &node->data.if_expr.cond);
        cc_ast_coalesce(state, &node->data.if_expr.then);
        cc_ast_coalesce(state, &node->data.if_expr.else_);
        break;
    case CC_AST_NODE_RETURN:
        cc_ast_coalesce(state, &node->data.retval);
        break;
    default:
        if (cc_ast_type_is_binop(node->type)) {
            cc_ast_coalesce(state, &node->data.binop.lhs);
            cc_ast_coalesce(state, &node->data.binop.rhs);
        }
        break;
    }
}
