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
    case CC_AST_NODE_LITERAL_FLOAT: return "float";
    case CC_AST_NODE_LITERAL_INTEGER: return "integer";
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

void cc_ast_dump(cc_state_t *state, cc_ast_node_ref_t ref) {
    cc_ast_node_t const *node = state->nodes + ref;
    printf("(%u:%s", (unsigned)ref, cc_ast_get_name(node->type));
    switch (node->type) {
    case CC_AST_NODE_NEW_TYPE:
        printf(" %s <", cc_strview_get(state, node->data.new_type.name));
        for (size_t i = 0; i < node->data.new_type.n_childs; ++i)
            cc_ast_dump(state, node->data.new_type.childs[i]);
        printf(">)");
        break;
    case CC_AST_NODE_NEW_VAR:
        printf(" %s ", cc_strview_get(state, node->data.new_var.name));
        cc_ast_dump(state, node->data.new_var.type_def);
        printf(")");
        break;
    case CC_AST_NODE_BLOCK:
        printf("=%zu <", node->data.block.n_childs);
        for (size_t i = 0; i < node->data.block.n_childs; ++i)
            cc_ast_dump(state, node->data.block.childs[i]);
        printf(">)");
        break;
    default:
        printf(")");
        break;
    }
}

cc_ast_node_ref_t cc_ast_push_node(cc_state_t *state, cc_ast_node_t node) {
    cc_ast_node_ref_t ref = state->n_nodes;
    state->nodes[state->n_nodes++] = node;
    return ref;
}

void cc_ast_pop_node(cc_state_t *state, cc_ast_node_ref_t ref) {
    assert(ref > 0);
    state->n_nodes = ref - 1;
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
