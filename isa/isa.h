#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 8192

#define FLAGS_BIT_N (0) /* Negative */
#define FLAGS_BIT_Z (1) /* Zero */
#define FLAGS_BIT_C (2) /* Carry */
#define FLAGS_BIT_V (3) /* Underflow */
#define FLAGS_BIT_O (4) /* Overflow */
#define FLAGS_BIT_PG (5) /* Paging enabled */

/* First 4 bits of the first instruction byte */
enum xm_cb0 {
    XM_CB_INTEGER,
    XM_CB_FLOAT,
    XM_CB_CONTROL,
    XM_CB_VECTOR,
    XM_CB_TILE,
    XM_CB_EXTENSION_5,
    XM_CB_EXTENSION_6,
    XM_CB_EXTENSION_7,
    XM_CB_EXTENSION_8,
    XM_CB_EXTENSION_9,
    XM_CB_EXTENSION_10,
    XM_CB_EXTENSION_11,
    XM_CB_EXTENSION_12,
    XM_CB_EXTENSION_13,
    XM_CB_EXTENSION_14,
    XM_CB_DEBUG,
};
/* Condition code */
enum xm_cb1 {
    CC_C, /* Carry */
    CC_NC, /* Not carry */
    CC_Z, /* Zero */
    CC_NZ,
    CC_ZC, /* Zero AND carry */
    CC_NZC, /* Not zero OR carry */
    CC_O, /* Overflow */
    CC_NO,
    CC_V, /* Underflow */
    CC_NV,
    CC_UNUSED_10,
    CC_UNUSED_11,
    CC_UNUSED_12,
    CC_UNUSED_13,
    CC_UNUSED_14,
    CC_UNUSED_15,
};

enum xm_inst_format {
    /* <Integer> Rd(4) Ra(4) Rb(4) Imm(4) Opcode(8)
        Memory Address = (Ra + Rb * Imm * 4); Value source/destination = Rd */
    XM_FORMAT_R4R4R4I4O8,
    /* <Integer> Rd(4) Ra(4) Imm(8) Opcode(8)
        Memory Address = (Ra + Imm * 4); Value source/destination = Rd */
    XM_FORMAT_R4R4I8O8,
    /* <Integer>
        If Opcode(8) high bit is set:
            Rd(4) Ra(4) Rb(4) Imm(4) Opcode(8)
        Else:
            Rd(4) Ra(4) Imm(8) Opcode(8)
    */
    XM_FORMAT_R4R4I8O8_IFHBS,
    /* <Integer> AbsoluteAddress(16) Opcode(8) */
    XM_FORMAT_AA16O8,
    /* <Integer> RelativeAddress(16) Opcode(8) */
    XM_FORMAT_RA16O8,
    /* <Integer> Rd(4) U(4) RelativeAddress(8) Opcode(8) */
    XM_FORMAT_R4U4RA8O8,
    /* <Integer> Rd(8) Ra(8) Opcode(8) */
    XM_FORMAT_R8R8RA8O8,
    /* <Integer> Unused(16) Opcode(8) */
    XM_FORMAT_U16O8,
};

static struct xm_inst_table_entry {
    const char *name;
    enum xm_inst_format format;
    uint32_t op;
} xm_inst_table[] = {
    {"add", XM_FORMAT_R4R4I8O8_IFHBS, 0x00},
    {"sub", XM_FORMAT_R4R4I8O8_IFHBS, 0x01},
    {"mul", XM_FORMAT_R4R4I8O8_IFHBS, 0x02},
    {"div", XM_FORMAT_R4R4I8O8_IFHBS, 0x03},
    {"rem", XM_FORMAT_R4R4I8O8_IFHBS, 0x04},
    {"imul", XM_FORMAT_R4R4I8O8_IFHBS, 0x05},
    {"and", XM_FORMAT_R4R4I8O8_IFHBS, 0x06},
    {"xor", XM_FORMAT_R4R4I8O8_IFHBS, 0x07},
    {"or", XM_FORMAT_R4R4I8O8_IFHBS, 0x08},
    {"shl", XM_FORMAT_R4R4I8O8_IFHBS, 0x09},
    {"shr", XM_FORMAT_R4R4I8O8_IFHBS, 0x0A},
    {"pcnt", XM_FORMAT_R4R4I8O8_IFHBS, 0x0B},
    {"clz", XM_FORMAT_R4R4I8O8_IFHBS, 0x0C},
    {"clo", XM_FORMAT_R4R4I8O8_IFHBS, 0x0D},
    {"bswap", XM_FORMAT_R4R4I8O8_IFHBS, 0x0E},
    {"ipcnt", XM_FORMAT_R4R4I8O8_IFHBS, 0x0F},
    {"stb", XM_FORMAT_R4R4I8O8_IFHBS, 0x10},
    {"stw", XM_FORMAT_R4R4I8O8_IFHBS, 0x11},
    {"stl", XM_FORMAT_R4R4I8O8_IFHBS, 0x12},
    {"stq", XM_FORMAT_R4R4I8O8_IFHBS, 0x13},
    {"ldb", XM_FORMAT_R4R4I8O8_IFHBS, 0x14},
    {"ldw", XM_FORMAT_R4R4I8O8_IFHBS, 0x15},
    {"ldl", XM_FORMAT_R4R4I8O8_IFHBS, 0x16},
    {"ldq", XM_FORMAT_R4R4I8O8_IFHBS, 0x17},
    {"lea", XM_FORMAT_R4R4I8O8_IFHBS, 0x18},
    /* 0x18 - 0x3F hole */
    {"jmp", XM_FORMAT_AA16O8, 0x40},
    {"jmprel", XM_FORMAT_RA16O8, 0x41},
    {"call", XM_FORMAT_R4U4RA8O8, 0x42},
    {"ret", XM_FORMAT_U16O8, 0x43},
    /* 0x44 - 0x4F hole */
    {"bz", XM_FORMAT_R4U4RA8O8, 0x50},
    {"bnz", XM_FORMAT_R4U4RA8O8, 0x51},
    {"bgtez", XM_FORMAT_R4U4RA8O8, 0x52},
    {"bltez", XM_FORMAT_R4U4RA8O8, 0x53},
    {"beq0", XM_FORMAT_R4U4RA8O8, 0x54},
    {"bne0", XM_FORMAT_R4U4RA8O8, 0x55},
    {"bgt0", XM_FORMAT_R4U4RA8O8, 0x56},
    {"blt0", XM_FORMAT_R4U4RA8O8, 0x57},
    {"bgte0", XM_FORMAT_R4U4RA8O8, 0x58},
    {"blte0", XM_FORMAT_R4U4RA8O8, 0x59},
    {"beq1", XM_FORMAT_R4U4RA8O8, 0x5a},
    {"bne1", XM_FORMAT_R4U4RA8O8, 0x5b},
    {"bgt1", XM_FORMAT_R4U4RA8O8, 0x5c},
    {"blt1", XM_FORMAT_R4U4RA8O8, 0x5d},
    {"bgte1", XM_FORMAT_R4U4RA8O8, 0x5e},
    {"blte1", XM_FORMAT_R4U4RA8O8, 0x5f},
    /* 0x46 - 0x4F hole */
};
#define XM_INST_TABLE_COUNT (sizeof(xm_inst_table) / sizeof(xm_inst_table[0]))

#define XM_PAGE_R 1 /* Read */
#define XM_PAGE_W 2 /* Write */
#define XM_PAGE_X 4 /* Execute */

/* Volatile registers $t0 - $t7 */
#define XM_ABI_T0 0
#define XM_ABI_T1 1
#define XM_ABI_T2 2
#define XM_ABI_T3 3
#define XM_ABI_T4 4
#define XM_ABI_T5 5
#define XM_ABI_T6 6
#define XM_ABI_T7 7

/* Parameter */
#define XM_ABI_A0 8 /* Return value is on $r8 */
#define XM_ABI_A1 9
#define XM_ABI_A2 10
#define XM_ABI_A3 11

#define XM_ABI_RA 12 /* Return address */
#define XM_ABI_BP 13 /* Base pointer */
#define XM_ABI_SP 14 /* Stack pointer */
#define XM_ABI_TP 15 /* Thread pointer */
