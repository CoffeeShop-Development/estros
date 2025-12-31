#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "isa.h"

#if 0
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#define OUT_MAX_SIZE (INT32_MAX)
struct asm_state {
    char const *in;
    size_t in_len;
    char *out;
    size_t out_len;
};
void asm_assemble(char const *in_path) {
    int fd = open(in_path, O_RDONLY);
    struct asm_state state;
    struct stat st;
    fstat(fd, &st);
    state.in = (char const*)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE | MAP_PREFAULT_READ, fd, 0);
    state.in_len = st.st_size;
    state.out = (char*)mmap(NULL, OUT_MAX_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    state.out_len = 0;
    close(fd);
}
#endif
/* Not worth to make a super optimized assembler, just whatever works :) */

#define LABEL_NAME_SIZE 32

struct operand {
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
    } type;
};

static struct {
    struct label {
        char name[LABEL_NAME_SIZE];
        unsigned long pc;
    } labels[512];
    unsigned n_labels;
    struct fixup {
        char name[LABEL_NAME_SIZE];
        enum fixup_type {
            FIXUP_NONE,
            /* Relative offset 16, size 8 */
            FIXUP_REL_O16S8,
        } type;
        unsigned long pc;
        unsigned long offset;
    } fixups[512];
    unsigned n_fixups;
    unsigned long pc;
    unsigned char buf[65536];
    unsigned long count;
} g_state = {0};

#define ASM_ERROR_IF(c, ...) \
    do if (c) \
        (void)fprintf(stderr, __FILE__ ": " #c " " __VA_ARGS__), (void)fprintf(stderr, "\n"); \
    while (0);

static struct operand asm_parse_operand(const char *p) {
    struct operand op = {0};
    if (*p == '$' && p[1] == 'c' && p[2] == 'r') {
        op.type = OP_CONTROL_REG;
        op.data.i = atoll(p + 3);
    } else if (*p == '$' && p[1] == 't' && p[2] == 'm') {
        op.type = OP_TILE_REG;
        op.data.i = atoll(p + 3);
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

static unsigned long asm_firstpass(char const *name, struct operand const op[]) {
    for (unsigned i = 0; i < XM_INST_TABLE_COUNT; ++i) {
        bool match = strcmp(name, xm_inst_table[i].name) == 0;
        unsigned char ob[8] = {0}, oc = 0;
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
            ob[0] = XM_CB_INTEGER;
            ob[1] = op[0].data.i & 0x0f;
            ob[2] = op[1].data.i & 0xff;
            ob[3] = xm_inst_table[i].op;
            oc = 4;
            if (op[1].type == OP_LABEL) {
                g_state.fixups[g_state.n_fixups].type = FIXUP_REL_O16S8;
                g_state.fixups[g_state.n_fixups].pc = g_state.pc;
                g_state.fixups[g_state.n_fixups].offset = g_state.count;
                strcpy(g_state.fixups[g_state.n_fixups].name, op[1].data.name);
                ++g_state.n_fixups;
            }
        } else if (match) {
            ASM_ERROR_IF(true, "unhandled type");
        }
        if (match) {
            ASM_ERROR_IF(oc == 0);
            memcpy(g_state.buf + g_state.count, ob, oc);
            g_state.count += oc;
            return oc;
        }
    }
    ASM_ERROR_IF(true, "unhandled memmonic <%s>", name);
    return 0;
}

static void asm_process_line(const char line[]) {
    char *p;
    /* Remove trailing characters */
    if ((p = strpbrk(line, "#\r\n")) != NULL) *p = '\0';
    if (*(p = line) != '\0') {
        char *label, *mem;
        while (isspace(*p)) ++p;
        if (*p == '\0') return;
        /* An actual line? skip whitespace... */
        if ((label = strchr(p, ':')) != NULL) {
            /* It's a label, cutoff non-ascii */
            if ((label = strpbrk(p, ": \t")) != NULL) *label = '\0';
            strcpy(g_state.labels[g_state.n_labels].name, p);
            g_state.labels[g_state.n_labels].pc = g_state.pc;
            ++g_state.n_labels;
            return;
        }
        mem = p; /* It's a memmonic */
        if ((p = strpbrk(p, " \t")) != NULL) {
            struct operand final_op[4] = {0};
            char *op[8] = {NULL};
            *(p++) = '\0';
            while (isspace(*p)) ++p;
            /* parse operands (variable length) */
            for (unsigned i = 0; i < sizeof(op) / sizeof(op[0]); ++i) {
                op[i] = p;
                if ((p = strchr(p, ',')) != NULL) {
                    *(p++) = '\0';
                } else
                    break;
            }
            for (unsigned i = 0; i < sizeof(op) / sizeof(op[0]); ++i)
                if (op[i] != NULL)
                    final_op[i] = asm_parse_operand(op[i]);
            g_state.pc += asm_firstpass(mem, final_op);
        } else {
            struct operand final_op[4] = {0};
            g_state.pc += asm_firstpass(mem, final_op);
        }
    }
}

void asm_assemble(FILE* fp) {
    char line[BUFSIZ];
    while (fgets(line, sizeof line, fp) != NULL)
        asm_process_line(line);
}

struct label asm_find_label(const char name[]) {
    for (unsigned i = 0; i < g_state.n_labels; ++i) {
        struct label *l = &g_state.labels[i];
        if (!strcmp(l->name, name))
            return *l;
    }
    fprintf(stderr, "label %s not found\n", name);
    abort();
}

static void asm_fixup() {
    for (unsigned i = 0; i < g_state.n_fixups; ++i) {
        struct fixup *f = &g_state.fixups[i];
        struct label l = asm_find_label(f->name);
        char ob[8], oc;
        switch (f->type) {
        case FIXUP_REL_O16S8: {
            int32_t rela = (int32_t)(l.pc - f->pc);
            ASM_ERROR_IF(rela < INT8_MIN || rela > INT8_MAX);
            /* 0<cb> 1<R8> 2<A8> 3<op> */
            g_state.buf[f->offset + 2] = (int8_t)rela;
            break;
        }
        default:
            abort();
        }
    }
}

int main(int argc, char *argv[]) {
    FILE* fp = stdin, *out = stdout;
    if (argc <= 2) {
        fprintf(stderr, "%s <in> <out>\n", argv[0]);
        return EXIT_FAILURE;
    }
    fp = fopen(argv[1], "rt");
    out = fopen(argv[2], "wt");
    /**/
    asm_assemble(fp);
    asm_fixup();
    fwrite(g_state.buf, 1, g_state.count, out);
    /**/
    fclose(out);
    fclose(fp);
    return EXIT_SUCCESS;
}
