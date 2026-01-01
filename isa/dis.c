#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "isa.h"

void dis_print_format(char ob[], const char* name, enum xm_inst_format f, FILE* out) {
    char buf[BUFSIZ];
    buf[0] = '\0';
    switch (f) {
    case XM_FORMAT_R4R4I8O8_IFHBS:
        if ((ob[3] & 0x80) != 0) {
            sprintf(buf, "$r%i,$r%i,%i",
                ob[1] & 0x0f, (ob[1] >> 4) & 0x0f,
                ob[2]);
        } else {
            sprintf(buf, "$r%i,$r%i,$r%i,%i",
                ob[1] & 0x0f, (ob[1] >> 4) & 0x0f,
                ob[2] & 0x0f, (ob[2] >> 4) & 0x0f);
        }
        break;
    case XM_FORMAT_R4U4RA8O8: {
        uint8_t cc = ob[1] >> 4;
        sprintf(buf, "$r%i,%i,?%c%c%c%c", ob[1] & 0x0f, (int32_t)(int8_t)ob[2],
            (cc & 0x01) != 0 ? '!' : ' ',
            (cc & 0x02) != 0 ? 'n' : ' ',
            (cc & 0x04) != 0 ? 'z' : ' ',
            (cc & 0x08) != 0 ? 'c' : ' '
        );
        break;
    }
    case XM_FORMAT_F4F4F4F4:
        sprintf(buf, "$f%i,$f%i,$f%i,$f%i",
            ob[1] & 0x0f, (ob[1] >> 4) & 0x0f,
            ob[2] & 0x0f, (ob[2] >> 4) & 0x0f);
        break;
    case XM_FORMAT_U16O8:
        break;
    default:
        strcpy(buf, "<invalid>");
        break;
    }
    fprintf(out, "\t%-8s%s\n", name, buf);
}

void dis_disassemble(FILE* fp, FILE* out) {
    char ob[8];
    while ((ob[0] = fgetc(fp)) != EOF) { /* control op */
        switch (ob[0] & 0x0f) {
        case XM_CB_INTEGER:
            ob[1] = fgetc(fp); /* d0 */
            ob[2] = fgetc(fp); /* d1 */
            ob[3] = fgetc(fp); /* opcode */
            for (unsigned i = 0; i < XM_INST_TABLE_COUNT; ++i)
                if (xm_inst_table[i].op == (ob[3] & 0x7f)) {
                    dis_print_format(ob, xm_inst_table[i].name, xm_inst_table[i].format, out);
                    break;
                }
            break;
        case XM_CB_FLOAT:
            break;
        case XM_CB_VECTOR:
            break;
        case XM_CB_TILE:
            break;
        default:
            abort();
        }
    }
}

int main(int argc, char *argv[]) {
    FILE* fp = stdin;
    if (argc > 1)
        fp = fopen(argv[1], "rb");
    /**/
    dis_disassemble(fp, stdout);
    /**/
    if (fp != stdin)
        fclose(fp);
    return EXIT_SUCCESS;
}
