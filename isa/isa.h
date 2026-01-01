#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 8192

#define FLAGS_BIT_N (1 << 0) /* Negative */
#define FLAGS_BIT_Z (1 << 1) /* Zero */
#define FLAGS_BIT_C (1 << 2) /* Carry */
#define FLAGS_BIT_V (1 << 3) /* Underflow */
#define FLAGS_BIT_O (1 << 4) /* Overflow */
#define FLAGS_BIT_PG (1 << 5) /* Paging enabled */

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
    /* <Float> Fd(4) Fa(4) Fb(4) Fc(4) */
    XM_FORMAT_F4F4F4F4,
    /* <Debug> */
    XM_FORMAT_D8
};

static enum xm_cb0 xm_get_cb0_from_format(enum xm_inst_format f) {
    switch (f) {
    case XM_FORMAT_R4R4R4I4O8:
    case XM_FORMAT_R4R4I8O8:
    case XM_FORMAT_R4R4I8O8_IFHBS:
    case XM_FORMAT_AA16O8:
    case XM_FORMAT_RA16O8:
    case XM_FORMAT_R4U4RA8O8:
    case XM_FORMAT_R8R8RA8O8:
    case XM_FORMAT_U16O8:
        return XM_CB_INTEGER;
    case XM_FORMAT_F4F4F4F4:
        return XM_CB_FLOAT;
    case XM_FORMAT_D8:
        return XM_CB_DEBUG;
    }
    return XM_CB_DEBUG;
}

#define XM_INST_LIST \
    XM_INST_ELEM(add, XM_FORMAT_R4R4I8O8_IFHBS, 0x00) \
    XM_INST_ELEM(sub, XM_FORMAT_R4R4I8O8_IFHBS, 0x01) \
    XM_INST_ELEM(mul, XM_FORMAT_R4R4I8O8_IFHBS, 0x02) \
    XM_INST_ELEM(div, XM_FORMAT_R4R4I8O8_IFHBS, 0x03) \
    XM_INST_ELEM(rem, XM_FORMAT_R4R4I8O8_IFHBS, 0x04) \
    XM_INST_ELEM(imul, XM_FORMAT_R4R4I8O8_IFHBS, 0x05) \
    XM_INST_ELEM(and, XM_FORMAT_R4R4I8O8_IFHBS, 0x06) \
    XM_INST_ELEM(xor, XM_FORMAT_R4R4I8O8_IFHBS, 0x07) \
    XM_INST_ELEM(or, XM_FORMAT_R4R4I8O8_IFHBS, 0x08) \
    XM_INST_ELEM(shl, XM_FORMAT_R4R4I8O8_IFHBS, 0x09) \
    XM_INST_ELEM(shr, XM_FORMAT_R4R4I8O8_IFHBS, 0x0A) \
    XM_INST_ELEM(pcnt, XM_FORMAT_R4R4I8O8_IFHBS, 0x0B) \
    XM_INST_ELEM(clz, XM_FORMAT_R4R4I8O8_IFHBS, 0x0C) \
    XM_INST_ELEM(clo, XM_FORMAT_R4R4I8O8_IFHBS, 0x0D) \
    XM_INST_ELEM(bswap, XM_FORMAT_R4R4I8O8_IFHBS, 0x0E) \
    XM_INST_ELEM(ipcnt, XM_FORMAT_R4R4I8O8_IFHBS, 0x0F) \
    XM_INST_ELEM(stb, XM_FORMAT_R4R4I8O8_IFHBS, 0x10) \
    XM_INST_ELEM(stw, XM_FORMAT_R4R4I8O8_IFHBS, 0x11) \
    XM_INST_ELEM(stl, XM_FORMAT_R4R4I8O8_IFHBS, 0x12) \
    XM_INST_ELEM(stq, XM_FORMAT_R4R4I8O8_IFHBS, 0x13) \
    XM_INST_ELEM(ldb, XM_FORMAT_R4R4I8O8_IFHBS, 0x14) \
    XM_INST_ELEM(ldw, XM_FORMAT_R4R4I8O8_IFHBS, 0x15) \
    XM_INST_ELEM(ldl, XM_FORMAT_R4R4I8O8_IFHBS, 0x16) \
    XM_INST_ELEM(ldq, XM_FORMAT_R4R4I8O8_IFHBS, 0x17) \
    XM_INST_ELEM(lea, XM_FORMAT_R4R4I8O8_IFHBS, 0x18) \
    /* 0x19 - 0x1F */ \
    XM_INST_ELEM(cmp, XM_FORMAT_R4R4I8O8_IFHBS, 0x20) \
    XM_INST_ELEM(cmpkp, XM_FORMAT_R4R4I8O8_IFHBS, 0x21) \
    /* 0x21 - 0x3F hole */ \
    XM_INST_ELEM(jmp, XM_FORMAT_AA16O8, 0x40) \
    XM_INST_ELEM(jmprel, XM_FORMAT_RA16O8, 0x41) \
    XM_INST_ELEM(call, XM_FORMAT_R4U4RA8O8, 0x42) \
    XM_INST_ELEM(ret, XM_FORMAT_U16O8, 0x43) \
    /* 0x44 - 0x4F hole */ \
    XM_INST_ELEM(bz, XM_FORMAT_R4U4RA8O8, 0x50) \
    XM_INST_ELEM(b, XM_FORMAT_R4U4RA8O8, 0x51) \
    XM_INST_ELEM(bgzs, XM_FORMAT_R4U4RA8O8, 0x52) \
    XM_INST_ELEM(bgpc, XM_FORMAT_R4U4RA8O8, 0x53) \
    XM_INST_ELEM(bgpcrela, XM_FORMAT_R4U4RA8O8, 0x54) \
    XM_INST_ELEM(bo, XM_FORMAT_R4U4RA8O8, 0x55) \
    XM_INST_ELEM(bgoz, XM_FORMAT_R4U4RA8O8, 0x56) \
    XM_INST_ELEM(bemax, XM_FORMAT_R4U4RA8O8, 0x57) \
    XM_INST_ELEM(bet0, XM_FORMAT_R4U4RA8O8, 0x58) \
    XM_INST_ELEM(bet1, XM_FORMAT_R4U4RA8O8, 0x59) \
    XM_INST_ELEM(bet2, XM_FORMAT_R4U4RA8O8, 0x5a) \
    XM_INST_ELEM(bet3, XM_FORMAT_R4U4RA8O8, 0x5b) \
    XM_INST_ELEM(bet4, XM_FORMAT_R4U4RA8O8, 0x5c) \
    XM_INST_ELEM(bet5, XM_FORMAT_R4U4RA8O8, 0x5d) \
    XM_INST_ELEM(bet6, XM_FORMAT_R4U4RA8O8, 0x5e) \
    XM_INST_ELEM(bet7, XM_FORMAT_R4U4RA8O8, 0x5f) \
    /* 0x46 - 0x4F hole */ \
    /* Floating point insn */ \
    XM_INST_ELEM(fadd3, XM_FORMAT_F4F4F4F4, 0x00) \
    XM_INST_ELEM(fsub3, XM_FORMAT_F4F4F4F4, 0x01) \
    XM_INST_ELEM(fdiv3, XM_FORMAT_F4F4F4F4, 0x02) \
    XM_INST_ELEM(fmul3, XM_FORMAT_F4F4F4F4, 0x03) \
    XM_INST_ELEM(fmod3, XM_FORMAT_F4F4F4F4, 0x04) \
    XM_INST_ELEM(fmadd, XM_FORMAT_F4F4F4F4, 0x05) \
    XM_INST_ELEM(fmsub, XM_FORMAT_F4F4F4F4, 0x06) \
    XM_INST_ELEM(fsqrt3, XM_FORMAT_F4F4F4F4, 0x07) \
    XM_INST_ELEM(fhyp, XM_FORMAT_F4F4F4F4, 0x08) \
    XM_INST_ELEM(fnorm, XM_FORMAT_F4F4F4F4, 0x09) \
    XM_INST_ELEM(fabs, XM_FORMAT_F4F4F4F4, 0x0a) \
    XM_INST_ELEM(fsign, XM_FORMAT_F4F4F4F4, 0x0b) \
    XM_INST_ELEM(fnabs, XM_FORMAT_F4F4F4F4, 0x0c) \
    XM_INST_ELEM(fcos, XM_FORMAT_F4F4F4F4, 0x0d) \
    XM_INST_ELEM(fsin, XM_FORMAT_F4F4F4F4, 0x0e) \
    XM_INST_ELEM(ftan, XM_FORMAT_F4F4F4F4, 0x0f) \
    XM_INST_ELEM(facos, XM_FORMAT_F4F4F4F4, 0x10) \
    XM_INST_ELEM(fatan, XM_FORMAT_F4F4F4F4, 0x11) \
    XM_INST_ELEM(fasin, XM_FORMAT_F4F4F4F4, 0x12) \
    XM_INST_ELEM(fcbrt, XM_FORMAT_F4F4F4F4, 0x13) \
    XM_INST_ELEM(fy0, XM_FORMAT_F4F4F4F4, 0x14) \
    XM_INST_ELEM(fy1, XM_FORMAT_F4F4F4F4, 0x15) \
    XM_INST_ELEM(fj0, XM_FORMAT_F4F4F4F4, 0x16) \
    XM_INST_ELEM(fj1, XM_FORMAT_F4F4F4F4, 0x17) \
    XM_INST_ELEM(fexp, XM_FORMAT_F4F4F4F4, 0x18) \
    XM_INST_ELEM(frsqrt, XM_FORMAT_F4F4F4F4, 0x19) \
    XM_INST_ELEM(frcbrt, XM_FORMAT_F4F4F4F4, 0x1a) \
    XM_INST_ELEM(fpow2, XM_FORMAT_F4F4F4F4, 0x1b) \
    XM_INST_ELEM(fpow3, XM_FORMAT_F4F4F4F4, 0x1c) \
    XM_INST_ELEM(fmax, XM_FORMAT_F4F4F4F4, 0x1d) \
    XM_INST_ELEM(fmin, XM_FORMAT_F4F4F4F4, 0x1e) \
    XM_INST_ELEM(fclamp, XM_FORMAT_F4F4F4F4, 0x1f) \
    XM_INST_ELEM(finv, XM_FORMAT_F4F4F4F4, 0x20) \
    XM_INST_ELEM(fconstpi, XM_FORMAT_F4F4F4F4, 0x21) \
    XM_INST_ELEM(fconste, XM_FORMAT_F4F4F4F4, 0x22) \
    XM_INST_ELEM(fconstpi2, XM_FORMAT_F4F4F4F4, 0x23) \
    XM_INST_ELEM(frad, XM_FORMAT_F4F4F4F4, 0x24) \
    XM_INST_ELEM(fdeg, XM_FORMAT_F4F4F4F4, 0x25) \
    XM_INST_ELEM(fsel, XM_FORMAT_F4F4F4F4, 0x26) \
    XM_INST_ELEM(fsel2, XM_FORMAT_F4F4F4F4, 0x27) \
    XM_INST_ELEM(fgamma, XM_FORMAT_F4F4F4F4, 0x28) \
    XM_INST_ELEM(flgamma, XM_FORMAT_F4F4F4F4, 0x29) \
    /* Complex ISA */ \
    XM_INST_ELEM(faddcrr, XM_FORMAT_F4F4F4F4, 0x30) \
    XM_INST_ELEM(fsubcrr, XM_FORMAT_F4F4F4F4, 0x31) \
    XM_INST_ELEM(fdivcrr, XM_FORMAT_F4F4F4F4, 0x32) \
    XM_INST_ELEM(fmulcrr, XM_FORMAT_F4F4F4F4, 0x33) \
    XM_INST_ELEM(fmodcrr, XM_FORMAT_F4F4F4F4, 0x34) \
    /* Hole */ \
    XM_INST_ELEM(faddcri, XM_FORMAT_F4F4F4F4, 0x40) \
    XM_INST_ELEM(fsubcri, XM_FORMAT_F4F4F4F4, 0x41) \
    XM_INST_ELEM(fdivcri, XM_FORMAT_F4F4F4F4, 0x42) \
    XM_INST_ELEM(fmulcri, XM_FORMAT_F4F4F4F4, 0x43) \
    XM_INST_ELEM(fmodcri, XM_FORMAT_F4F4F4F4, 0x44) \
    /* Debug functions */ \
    XM_INST_ELEM(halt, XM_FORMAT_D8, 0x0f) \

static struct xm_inst_table_entry {
    const char *name;
    enum xm_inst_format format;
    uint32_t op;
} xm_inst_table[] = {
#define XM_INST_ELEM(NAME, FORMAT, OP) {#NAME, FORMAT, OP},
    XM_INST_LIST
#undef XM_INST_ELEM
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
