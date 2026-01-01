#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "isa.h"

#define OUT_MAX_SIZE (INT32_MAX)
#define LIST_MAX_SIZE (65536)

#define LABEL_NAME_SIZE 16

struct asm_operand {
    union {
        long long i;
        char name[LABEL_NAME_SIZE];
    } data;
    enum {
        OP_INVALID,
        OP_REG,
        OP_FLOAT_REG,
        OP_VECTOR_REG,
        OP_CONTROL_REG,
        OP_TILE_REG,
        OP_IMM,
        OP_LABEL,
        OP_COND,
    } type;
};

typedef struct asm_state {
    char *in;
    size_t in_len;
    char *out;
    size_t out_len;

    struct asm_label {
        char name[LABEL_NAME_SIZE];
        uint32_t pc;
    } *labels;
    size_t n_labels;

    struct asm_fixup {
        char name[LABEL_NAME_SIZE];
        enum asm_fixup_type {
            FIXUP_NONE,
            /* Relative offset 16, size 8 */
            FIXUP_REL_O16S8,
        } type;
        uint32_t pc;
        uint32_t offset;
    } *fixups;
    size_t n_fixups;

    uint32_t pc;

    int fd_in;
    int fd_out;
} asm_state_t;

#define ASM_ERROR_IF(c, ...) \
    do if (c) \
        (void)fprintf(stderr, __FILE__ ": " #c " " __VA_ARGS__), (void)fprintf(stderr, "\n"); \
    while (0);

static struct asm_operand asm_parse_asm_operand(const char *p) {
    struct asm_operand op = {0};
    if (*p == '$' && p[1] == 'c' && p[2] == 'r') {
        op.type = OP_CONTROL_REG;
        op.data.i = atoll(p + 3);
    } else if (*p == '$' && p[1] == 't' && p[2] == 'm') {
        op.type = OP_TILE_REG;
        op.data.i = atoll(p + 3);
    } else if (*p == '?') {
        op.type = OP_COND;
        ++p;
        if (*p == '!') op.data.i |= 0x01, ++p;
        if (*p == 'n') op.data.i |= 0x02, ++p;
        if (*p == 'z') op.data.i |= 0x04, ++p;
        if (*p == 'c') op.data.i |= 0x08, ++p;

        /* g = !(C | Z) */
        if (*p == 'g') op.data.i |= 0x08 | 0x04 | 0x01, ++p;
        /* l = C */
        if (*p == 'l') op.data.i |= 0x08, ++p;
        /* e = Z */
        if (*p == 'e') op.data.i |= 0x04, ++p;
    /* Stack pointer */
    } else if (*p == '$' && p[1] == 's' && p[2] == 'p') {
        op.type = OP_REG;
        op.data.i = XM_ABI_SP;
    /* Base pointer */
    } else if (*p == '$' && p[1] == 'b' && p[2] == 'p') {
        op.type = OP_REG;
        op.data.i = XM_ABI_BP;
    /* Thread pointer */
    } else if (*p == '$' && p[1] == 't' && p[2] == 'p') {
        op.type = OP_REG;
        op.data.i = XM_ABI_TP;
    /* Return pointer */
    } else if (*p == '$' && p[1] == 'r' && p[2] == 'a') {
        op.type = OP_REG;
        op.data.i = XM_ABI_RA;
    /* aliases */
    } else if (*p == '$' && p[1] == 't') {
        op.type = OP_REG;
        op.data.i = atoll(p + 2) + XM_ABI_T0;
        ASM_ERROR_IF(op.data.i < XM_ABI_T0 || op.data.i > XM_ABI_T7);
    } else if (*p == '$' && p[1] == 'a') {
        op.type = OP_REG;
        op.data.i = atoll(p + 2) + XM_ABI_A0;
        ASM_ERROR_IF(op.data.i < XM_ABI_A0 || op.data.i > XM_ABI_A3);
    /* Special regs */
    } else if (*p == '$' && p[1] == 'r') {
        op.type = OP_REG;
        op.data.i = atoll(p + 2);
    } else if (*p == '$' && p[1] == 'v') {
        op.type = OP_VECTOR_REG;
        op.data.i = atoll(p + 2);
    } else if (*p == '$' && p[1] == 'f') {
        op.type = OP_FLOAT_REG;
        op.data.i = atoll(p + 2);
    } else if (isdigit(*p) || *p == '-') {
        op.type = OP_IMM;
        op.data.i = atoll(p);
    } else {
        op.type = OP_LABEL;
        strcpy(op.data.name, p);
    }
    return op;
}

static uint32_t asm_firstpass(asm_state_t* state, char const *name, struct asm_operand const op[]) {
    for (size_t i = 0; i < XM_INST_TABLE_COUNT; ++i) {
        bool match = strcmp(name, xm_inst_table[i].name) == 0;
        uint8_t ob[8] = {0}, oc = 0;
        if (match && xm_inst_table[i].format == XM_FORMAT_R4R4I8O8_IFHBS) {
            ASM_ERROR_IF(op[0].type != OP_REG);
            ASM_ERROR_IF(op[1].type != OP_REG);
            ob[0] = XM_CB_INTEGER;
            ob[1] = (op[0].data.i & 0x0f) | ((op[1].data.i & 0x0f) << 4);
            ob[3] = xm_inst_table[i].op & 0xff;
            if (op[2].type == OP_IMM) {
                /* Rd(4) Ra(4) Imm(8) Opcode(8) */
                ob[2] = op[2].data.i & 0xff;
                ob[3] |= 0x80;
            } else {
                /* Rd(4) Ra(4) Rb(4) Imm(4) Opcode(8) */
                ASM_ERROR_IF(op[2].type != OP_REG);
                ob[2] = (op[2].data.i & 0x0f)
                    | ((op[3].data.i & 0x0f) << 4);
            }
            oc = 4;
        } else if (match && xm_inst_table[i].format == XM_FORMAT_U16O8) {
            ob[0] = XM_CB_INTEGER;
            ob[1] = ob[2] = 0;
            ob[3] = xm_inst_table[i].op;
            oc = 4;
        } else if (match && xm_inst_table[i].format == XM_FORMAT_R4U4RA8O8) {
            ASM_ERROR_IF(op[0].type != OP_REG);
            ASM_ERROR_IF(op[1].type != OP_IMM && op[1].type != OP_LABEL);
            ASM_ERROR_IF(op[2].type != OP_COND);
            ob[0] = XM_CB_INTEGER;
            ob[1] = op[0].data.i & 0x0f | (op[2].data.i << 4);
            ob[2] = op[1].data.i & 0xff;
            ob[3] = xm_inst_table[i].op;
            oc = 4;
            if (op[1].type == OP_LABEL) {
                state->fixups[state->n_fixups].type = FIXUP_REL_O16S8;
                state->fixups[state->n_fixups].pc = state->pc;
                state->fixups[state->n_fixups].offset = state->out_len;
                strcpy(state->fixups[state->n_fixups].name, op[1].data.name);
                ++state->n_fixups;
            }
        } else if (match && xm_inst_table[i].format == XM_FORMAT_R4R4R4R4) {
            ASM_ERROR_IF(op[0].type != OP_REG);
            ASM_ERROR_IF(op[1].type != OP_REG);
            ASM_ERROR_IF(op[2].type != OP_REG);
            ASM_ERROR_IF(op[3].type != OP_REG);
            ob[0] = XM_CB_INTEGER;
            ob[1] = op[0].data.i & 0x0f | (op[1].data.i << 4);
            ob[2] = op[2].data.i & 0x0f | (op[3].data.i << 4);
            ob[3] = xm_inst_table[i].op;
            oc = 4;
        } else if (match && xm_inst_table[i].format == XM_FORMAT_F4F4F4F4) {
            ASM_ERROR_IF(op[0].type != OP_FLOAT_REG);
            ASM_ERROR_IF(op[1].type != OP_FLOAT_REG);
            ASM_ERROR_IF(op[2].type != OP_FLOAT_REG);
            ASM_ERROR_IF(op[3].type != OP_FLOAT_REG);
            ob[0] = XM_CB_FLOAT;
            ob[1] = op[0].data.i & 0x0f | (op[1].data.i << 4);
            ob[2] = op[2].data.i & 0x0f | (op[3].data.i << 4);
            ob[3] = xm_inst_table[i].op;
            oc = 4;
        } else if (match) {
            ASM_ERROR_IF(true, "unhandled type");
        }
        if (match) {
            ASM_ERROR_IF(oc == 0);
            memcpy(state->out + state->out_len, ob, oc);
            state->out_len += oc;
            return oc;
        }
    }
    ASM_ERROR_IF(true, "unhandled memmonic <%s>", name);
    return 0;
}

static void asm_process_line(asm_state_t* state, char line[]) {
    char *p;
    /* Remove trailing characters */
    if ((p = strpbrk(line, "#\r\n")) != NULL) *p = '\0';
    if (*(p = line) != '\0') {
        char *lab, *mem;
        while (isspace(*p)) ++p;
        if (*p == '\0') return;
        /* An actual line? skip whitespace... */
        if ((lab = strchr(p, ':')) != NULL) {
            /* It's a asm_label, cutoff non-ascii */
            if ((lab = strpbrk(p, ": \t")) != NULL) *lab = '\0';
            strcpy(state->labels[state->n_labels].name, p);
            state->labels[state->n_labels].pc = state->pc;
            ++state->n_labels;
            return;
        }
        mem = p; /* It's a memmonic */
        if ((p = strpbrk(p, " \t")) != NULL) {
            struct asm_operand final_op[4] = {0};
            char *op[8] = {NULL};
            *(p++) = '\0';
            while (isspace(*p)) ++p;
            /* parse asm_operands (variable length) */
            for (size_t i = 0; i < sizeof(op) / sizeof(op[0]); ++i) {
                op[i] = p;
                if ((p = strchr(p, ',')) != NULL) {
                    *(p++) = '\0';
                } else
                    break;
            }
            for (size_t i = 0; i < sizeof(op) / sizeof(op[0]); ++i)
                if (op[i] != NULL)
                    final_op[i] = asm_parse_asm_operand(op[i]);
            state->pc += asm_firstpass(state, mem, final_op);
        } else {
            struct asm_operand final_op[4] = {0};
            state->pc += asm_firstpass(state, mem, final_op);
        }
    }
}

#define MIN(A, B) (((A) > (B)) ? (B) : (A))
void asm_assemble(asm_state_t* state) {
    for (size_t i = 0; i < state->in_len; ) {
        const char *p = memchr(state->in + i, '\n', state->in_len - i);
        if (p != NULL) {
            char line[BUFSIZ] = {0};
            ptrdiff_t d = (ptrdiff_t)((p + 1) - (state->in + i));
            memcpy(line, state->in + i, (size_t)d);
            asm_process_line(state, line);
            i += (size_t)d;
            continue;
        } else {
            char line[BUFSIZ] = {0};
            memcpy(line, state->in + i, state->in_len - i);
            line[state->in_len - i + 1] = '\0';
            asm_process_line(state, line);
            break;
        }
    }
}

struct asm_label asm_find_label(asm_state_t* state, const char name[]) {
    for (size_t i = 0; i < state->n_labels; ++i) {
        struct asm_label *l = &state->labels[i];
        if (!strcmp(l->name, name))
            return *l;
    }
    fprintf(stderr, "asm_label %s not found\n", name);
    abort();
}

static void asm_fixup(asm_state_t* state) {
    for (size_t i = 0; i < state->n_fixups; ++i) {
        struct asm_fixup *f = &state->fixups[i];
        struct asm_label l = asm_find_label(state, f->name);
        char ob[8], oc;
        switch (f->type) {
        case FIXUP_REL_O16S8: {
            int32_t rela = (int32_t)(l.pc - f->pc);
            ASM_ERROR_IF(rela < INT8_MIN || rela > INT8_MAX);
            /* 0<cb> 1<R8> 2<A8> 3<op> */
            state->out[f->offset + 2] = (int8_t)rela;
            break;
        }
        default:
            abort();
        }
    }
}

static void asm_initialise(asm_state_t* state) {
    state->out = mmap(NULL, OUT_MAX_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    state->out_len = 0;
    state->labels = mmap(NULL, LIST_MAX_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    state->n_labels = 0;
    state->fixups = mmap(NULL, LIST_MAX_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    state->n_fixups = 0;
}

static void asm_add_input_file(asm_state_t* state, int fd) {
    struct stat st;
    fstat(fd, &st);
    state->in = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE | MAP_PREFAULT_READ, fd, 0);
    state->in_len = st.st_size;
    state->fd_in = fd;
}

static void asm_add_output_file(asm_state_t* state, int fd) {
    state->fd_out = fd;
}

static void asm_finish(asm_state_t* state) {
    write(state->fd_out, state->out, state->out_len);
    close(state->fd_out);
    close(state->fd_in);

    munmap(state->in, state->in_len);
    munmap(state->out, OUT_MAX_SIZE);
    munmap(state->fixups, LIST_MAX_SIZE);
    munmap(state->labels, LIST_MAX_SIZE);
}

int main(int argc, char *argv[]) {
    static struct asm_state state = {0};
    int fd_in = fileno(stdin), fd_out = fileno(stdout);
    if (argc <= 2) {
        fprintf(stderr, "%s <in> <out>\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd_in = open(argv[1], O_RDONLY);
    fd_out = open(argv[2], O_CREAT | O_TRUNC | O_WRONLY, 0755);
    asm_initialise(&state);
    asm_add_input_file(&state, fd_in);
    asm_add_output_file(&state, fd_out);
    asm_assemble(&state);
    asm_fixup(&state);
    asm_finish(&state);
    return EXIT_SUCCESS;
}
