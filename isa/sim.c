#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "isa.h"

#define SIM_RAM_BASE 0xF0000000
#define SIM_ROM_BASE 0x8000

#define SIM_RAM_SIZE (PAGE_SIZE * 512)
#define SIM_ROM_SIZE (PAGE_SIZE * 16)

typedef enum {
    CPUE_CONTINUE,
    CPUE_HALT,
} cpu_execute_result_t;
typedef enum {
    SIM_OPT_QUIET = 1 << 0,
    SIM_OPT_TEST = 1 << 1,
    SIM_OPT_TRACE_MEM = 1 << 2,
} sim_options_t;
typedef struct {
    struct cpu_state {
        /* Instruction pointer / Program counter */
        uint32_t pc;
        uint32_t flags;
        /* Control registers */
        uint32_t cr[16];
        /* 16 32-bit integer registers */
        uint32_t r[16];
        /* 16 32-bit floating point registers */
        float f[16];
        /* 16 128-bit vector registers */
        uint64_t v[16][2];
        /* 16 matrix tile registers */
        float tile[16][4 * 4];
    } cpu;

    sim_options_t opt;

    /* Perf counters */
    struct {
        unsigned long ticks;
        unsigned long b_misses;
        unsigned long b_taken;
        unsigned long jumps;
        unsigned long reads;
        unsigned long writes;
    } perf;

    /* Emulated memory */
    uint8_t trap_page[PAGE_SIZE];
    uint8_t ram[SIM_RAM_SIZE];
    uint8_t rom[SIM_ROM_SIZE];
} sim_state_t;

static void *cpu_translate(sim_state_t* sim, uint32_t a, int p) {
    if ((sim->opt & SIM_OPT_TRACE_MEM) != 0) {
        printf("%8x %c%c%c\n", a,
            (p & XM_PAGE_R) ? 'R' : '.',
            (p & XM_PAGE_W) ? 'W' : '.',
            (p & XM_PAGE_X) ? 'X' : '.');
    }
    if (a >= SIM_ROM_BASE && a < SIM_ROM_BASE + SIM_ROM_SIZE)
        return (void*)(sim->rom + a - SIM_ROM_BASE);
    else if (a >= SIM_RAM_BASE && a < SIM_RAM_BASE + SIM_RAM_SIZE)
        return (void*)(sim->ram + a - SIM_RAM_BASE);
    return sim->trap_page + (a % PAGE_SIZE);
}
static uint8_t cpu_read8(sim_state_t* sim, uint32_t addr) {
    ++sim->perf.reads;
    return *(uint8_t const*)cpu_translate(sim, addr, XM_PAGE_R);
}
static void cpu_write8(sim_state_t* sim, uint32_t addr, uint8_t v) {
    ++sim->perf.writes;
    *(uint8_t*)cpu_translate(sim, addr, XM_PAGE_W) = v;
}

static uint16_t cpu_read16(sim_state_t* sim, uint32_t addr) {
    return ((uint16_t)cpu_read8(sim, addr + 0) << 8)
        | ((uint16_t)cpu_read8(sim, addr + 1) << 0);
}
static void cpu_write16(sim_state_t* sim, uint32_t addr, uint16_t v) {
    cpu_write8(sim, addr + 0, v >> 0);
    cpu_write8(sim, addr + 1, v >> 8);
}

static uint32_t cpu_read32(sim_state_t* sim, uint32_t addr) {
    return ((uint32_t)cpu_read16(sim, addr + 0) << 16)
        | ((uint32_t)cpu_read16(sim, addr + 2) << 0);
}
static void cpu_write32(sim_state_t* sim, uint32_t addr, uint32_t v) {
    cpu_write16(sim, addr, v >> 0);
    cpu_write16(sim, addr + 2, v >> 16);
}

void cpu_debug_print(sim_state_t* sim) {
    if ((sim->opt & SIM_OPT_QUIET) != 0)
        return;
    printf("tick#%lu: pc=%08x :: Read=%lu, Written=%lu, B-Taken=%lu, B-Miss=%lu, Jumps=%lu\n",
        sim->perf.ticks, sim->cpu.pc,
        sim->perf.reads, sim->perf.writes, sim->perf.b_taken, sim->perf.b_misses, sim->perf.jumps);
    for (unsigned i = 0; i < 16; ++i)
        printf("$r%-2i: %8x%c", i, sim->cpu.r[i], ((i + 1) % 4 == 0) ? '\n' : ' ');
#if 0
    for (unsigned i = 0; i < 16; ++i)
        printf("$f%-2i: %8x%c", i, *(uint32_t const*)(&sim->cpu.f[i]), ((i + 1) % 4 == 0) ? '\n' : ' ');
#endif
}
static void cpu_dump_bytes(sim_state_t* sim, uint32_t addr, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i)
        printf("%02x%c", cpu_read8(sim, addr + i), (i + 1) % 32 == 0 ? '\n' : ' ');
}

/* Bit population count */
static uint32_t cpu_i_popcount(uint32_t v) {
    uint32_t c = 0;
    for (uint32_t i = 0; i < 32; ++i)
        if ((v & (1 << i)) != 0)
            ++c;
    return c;
}
/* Count leading zeroes */
static uint32_t cpu_i_clz(uint32_t v) {
    uint32_t c = 0;
    for (uint32_t i = 0; i < 32; ++i) {
        if ((v & (1 << i)) != 0)
            break;
        ++c;
    }
    return c;
}
/* Count leading ones */
static uint32_t cpu_i_clo(uint32_t v) {
    uint32_t c = 0;
    for (uint32_t i = 0; i < 32; ++i) {
        if ((v & (1 << i)) != 0)
            break;
        ++c;
    }
    return c;
}
static uint32_t cpu_i_bswap(uint32_t v) {
    return (((v >> 24) & 0xff) << 0)
        | (((v >> 16) & 0xff) << 8)
        | (((v >> 8) & 0xff) << 16)
        | (((v >> 0) & 0xff) << 24);
}

static uint32_t cpu_i_add32(sim_state_t* sim, uint32_t a, uint32_t b) {
    uint64_t result = (uint64_t)a + (uint64_t)b;
    sim->cpu.flags &= ~FLAGS_BIT_C;
    sim->cpu.flags |= (result & ~UINT32_MAX) != 0 ? FLAGS_BIT_C : 0;
    return (uint32_t)result;
}
static int32_t cpu_i_sub32(sim_state_t* sim, int32_t a, int32_t b) {
    int64_t result = (int64_t)a - (int64_t)b;
    sim->cpu.flags &= ~FLAGS_BIT_C;
    sim->cpu.flags |= (result & ~UINT32_MAX) != 0 ? FLAGS_BIT_C : 0;
    return (int32_t)result;
}

static uint32_t cpu_do_basic_alu(sim_state_t* sim, uint8_t op, uint32_t a, uint32_t b) {
    switch (op & 0x7f) {
    case 0x00: return cpu_i_add32(sim, a, b);
    case 0x01: return cpu_i_sub32(sim, a, b);
    case 0x02: return a * b;
    case 0x03: return a / b;
    case 0x04: return a % b;
    case 0x05: return (int32_t)a * (int32_t)b;
    case 0x06: return a & b;
    case 0x07: return a ^ b;
    case 0x08: return a | b;
    case 0x09: return a << b;
    case 0x0a: return a >> b;
    case 0x0b: return cpu_i_popcount(a + b);
    case 0x0c: return cpu_i_clz(a + b);
    case 0x0d: return cpu_i_clo(a + b);
    case 0x0e: return cpu_i_bswap(a + b);
    case 0x0f: return 32 - cpu_i_popcount(a + b);
    default: fprintf(stderr, "unknown %x\n", op); break;
    }
}

static void cpu_do_alu1(sim_state_t* sim, uint8_t op, uint32_t addr, uint8_t rd, uint32_t a, uint32_t b) {
    switch (op & 0x7f) {
    case 0x10: cpu_write8(sim, addr, sim->cpu.r[rd]); break;
    case 0x11: cpu_write16(sim, addr, sim->cpu.r[rd]); break;
    case 0x12: cpu_write32(sim, addr, sim->cpu.r[rd]); break;
    /*case 0x13: cpu_write32(sim, addr, sim->cpu.r[rd]); break;*/
    case 0x14: sim->cpu.r[rd] = cpu_read8(sim, addr); break;
    case 0x15: sim->cpu.r[rd] = cpu_read16(sim, addr); break;
    case 0x16: sim->cpu.r[rd] = cpu_read32(sim, addr); break;
    /*case 0x17: cpu_read32(sim, addr, sim->cpu.r[rd]); break;*/
    case 0x18: /* lea */
        sim->cpu.r[rd] = addr;
        break;
    case 0x20: { /* cmp */
        uint32_t r = cpu_i_add32(sim, a, b);
        sim->cpu.flags &= ~(FLAGS_BIT_Z | FLAGS_BIT_N);
        sim->cpu.flags |= r == 0 ? FLAGS_BIT_Z : 0;
        sim->cpu.flags |= (int32_t)r < 0 ? FLAGS_BIT_N : 0;
        sim->cpu.r[rd] = sim->cpu.flags;
        break;
    }
    case 0x21: { /* cmpkp */
        uint32_t old_flags = sim->cpu.flags;
        uint32_t r = cpu_i_add32(sim, a, b);
        sim->cpu.flags &= ~(FLAGS_BIT_Z | FLAGS_BIT_N);
        sim->cpu.flags |= r == 0 ? FLAGS_BIT_Z : 0;
        sim->cpu.flags |= (int32_t)r < 0 ? FLAGS_BIT_N : 0;
        sim->cpu.r[rd] = sim->cpu.flags;
        /* restore flags */
        sim->cpu.flags = old_flags;
        break;
    }
    default:
        sim->cpu.r[rd] = cpu_do_basic_alu(sim, op, a, b);
        sim->cpu.flags &= ~(FLAGS_BIT_Z | FLAGS_BIT_N);
        sim->cpu.flags |= sim->cpu.r[rd] == 0 ? FLAGS_BIT_Z : 0;
        sim->cpu.flags |= (int32_t)sim->cpu.r[rd] < 0 ? FLAGS_BIT_N : 0;   
        break;
    }
}

static float cpu_i_maxf(float a, float b) {
    return a > b ? a : b;
}
static float cpu_i_minf(float a, float b) {
    return a < b ? a : b;
}
static float cpu_i_clampf(float a, float low, float upper) {
    return cpu_i_minf(cpu_i_maxf(a, low), upper);
}

float cpu_do_fpu1(sim_state_t* sim, uint8_t op, uint8_t ra, uint8_t rb, uint8_t rc) {
    float a = sim->cpu.f[ra];
    float b = sim->cpu.f[rb];
    float c = sim->cpu.f[rc];
    switch (op) {
    case 0x00: return (a + b) + c;
    case 0x01: return (a + b) - c;
    case 0x02: return (a + b) / c;
    case 0x03: return (a + b) * c;
    case 0x04: return fmodf(a + b, c);
    case 0x05: return a + b * c;
    case 0x06: return a - b * c;
    case 0x07: return sqrtf(a + b + c);
    case 0x08: return hypotf(a + b, c);
    case 0x09: return sqrtf(a * a + b * b + c * c);
    case 0x0a: return fabs(a + b + c);
    case 0x0b: return signbit(a + b + c);
    case 0x0c: return -fabs(a + b + c);
    case 0x0d: return cosf(a + b + c);
    case 0x0e: return sinf(a + b + c);
    case 0x0f: return tanf(a + b + c);
    case 0x10: return acosf(a + b + c);
    case 0x11: return asinf(a + b + c);
    case 0x12: return atanf(a + b + c);
    case 0x13: return cbrtf(a + b + c);
    case 0x14: return y0(a + b + c);
    case 0x15: return y1(a + b + c);
    case 0x16: return j0(a + b + c);
    case 0x17: return j1(a + b + c);
    case 0x18: return expf(a + b + c);
    case 0x19: return 1.f / sqrtf(a + b + c);
    case 0x1a: return 1.f / cbrtf(a + b + c);
    case 0x1b: return powf(a + b, c);
    case 0x1c: return powf(powf(a, b), c);
    case 0x1d: return cpu_i_maxf(a + b, c);
    case 0x1e: return cpu_i_minf(a + b, c);
    case 0x1f: return cpu_i_clampf(a, b, c);
    case 0x20: return 1.f / (a + b + c);
    case 0x21: return M_PI * (a + b + c);
    case 0x22: return M_E * (a + b + c);
    case 0x23: return M_PI_2 * (a + b + c);
    case 0x24: return (a + b + c) * M_PI / 180.f;
    case 0x25: return (a + b + c) * 180.f / M_PI;
    case 0x26: return a > b ? c : 0.f;
    case 0x27: return a + b > 0.f ? c : 0.f;
    case 0x28: return gammaf(a + b + c);
    case 0x29: return lgammaf(a + b + c);
    /* complex isa is TODO */
    default: return 0.f;
    }
}

cpu_execute_result_t cpu_step(sim_state_t* sim) {
    uint8_t id[8]; /* Instruction decode */

    ++sim->perf.ticks;

    id[0] = cpu_read8(sim, sim->cpu.pc);
    switch (id[0] & 0x0f) {
    case XM_CB_INTEGER: {
        id[1] = cpu_read8(sim, sim->cpu.pc + 1);
        id[2] = cpu_read8(sim, sim->cpu.pc + 2);
        id[3] = cpu_read8(sim, sim->cpu.pc + 3);

        uint8_t op = id[3];
        if (op == 0x40) { /* jmp */
            sim->cpu.pc = (((uint32_t)id[1]) << 8) | id[2];
            ++sim->perf.jumps;
            return CPUE_CONTINUE;
        } else if (op == 0x41) { /* jmprel */
            int32_t rela = (int32_t)(int16_t)((((uint16_t)id[1]) << 8) | id[2]);
            sim->cpu.pc += rela;
            ++sim->perf.jumps;
            return CPUE_CONTINUE;
        } else if (op == 0x42) { /* call */
            uint8_t ra = id[1];
            int32_t rela = (int32_t)(int8_t)id[2];
            sim->cpu.pc = sim->cpu.r[ra] + rela;
            sim->cpu.r[XM_ABI_RA] = sim->cpu.pc + 4;
            ++sim->perf.jumps;
            return CPUE_CONTINUE;
        } else if (op == 0x43) { /* ret */
            sim->cpu.pc = sim->cpu.r[XM_ABI_RA];
            ++sim->perf.jumps;
            return CPUE_CONTINUE;
        } else if (op >= 0x50 && op <= 0x5F) { /* b */
            uint8_t ra = id[1] & 0x0f;
            uint8_t cc = (id[1] >> 4);
            int32_t rela = (int32_t)(int8_t)id[2];
            bool cond = 0;
            switch ((op - 0x50) & 0x0f) {
            case 0: cond = sim->cpu.r[ra] == 0; break;
            case 1: cond = true; break;
            case 2: cond = (int32_t)sim->cpu.r[ra] > 0; break;
            case 3: cond = sim->cpu.r[ra] > sim->cpu.pc; break;
            case 4: cond = sim->cpu.r[ra] > sim->cpu.pc + rela; break;
            case 5: cond = sim->cpu.r[ra] == 1; break;
            case 6: cond = (int32_t)sim->cpu.r[ra] > 1; break;
            case 7: cond = sim->cpu.r[ra] == ~0; break;
            case 8: cond = sim->cpu.r[ra] == sim->cpu.r[XM_ABI_T0]; break;
            case 9: cond = sim->cpu.r[ra] == sim->cpu.r[XM_ABI_T1]; break;
            case 10: cond = sim->cpu.r[ra] == sim->cpu.r[XM_ABI_T2]; break;
            case 11: cond = sim->cpu.r[ra] == sim->cpu.r[XM_ABI_T3]; break;
            case 12: cond = sim->cpu.r[ra] == sim->cpu.r[XM_ABI_T4]; break;
            case 13: cond = sim->cpu.r[ra] == sim->cpu.r[XM_ABI_T5]; break;
            case 14: cond = sim->cpu.r[ra] == sim->cpu.r[XM_ABI_T6]; break;
            case 15: cond = sim->cpu.r[ra] == sim->cpu.r[XM_ABI_T7]; break;
            }
            /* !, N, Z, C */
            cond = (cc & 0x02) != 0 ? (cond && (sim->cpu.flags & FLAGS_BIT_N) != 0) : cond;
            cond = (cc & 0x04) != 0 ? (cond && (sim->cpu.flags & FLAGS_BIT_Z) != 0) : cond;
            cond = (cc & 0x08) != 0 ? (cond && (sim->cpu.flags & FLAGS_BIT_C) != 0) : cond;
            cond = (cc & 0x01) != 0 ? !cond : cond; /* Invert condition flag */
            sim->cpu.pc += cond ? rela : 4;
            cond ? ++sim->perf.b_taken : ++sim->perf.b_misses;
            return CPUE_CONTINUE;
        } else if ((op & 0x80) != 0) {
            uint8_t rd = id[1] & 0x0f;
            uint8_t ra = (id[1] >> 4) & 0x0f;
            uint8_t imm = id[2];
            uint32_t addr = sim->cpu.r[ra] + imm * 4;
            cpu_do_alu1(sim, op, addr, rd, sim->cpu.r[ra], imm);
        } else {
            uint8_t rd = id[1] & 0x0f;
            uint8_t ra = (id[1] >> 4) & 0x0f;
            uint8_t rb = id[2] & 0x0f;
            uint8_t imm = (id[2] >> 4) & 0x0f;
            uint32_t addr = sim->cpu.r[ra] + sim->cpu.r[rb] * imm * 4;
            cpu_do_alu1(sim, op, addr, rd, sim->cpu.r[ra], sim->cpu.r[rb] + imm);
        }
        sim->cpu.pc += 4;
        return CPUE_CONTINUE;
    }
    case XM_CB_FLOAT: {
        uint8_t op = id[3];
        uint8_t rd = id[1] & 0x0f;
        uint8_t ra = (id[1] >> 4) & 0x0f;
        uint8_t rb = id[2] & 0x0f;
        uint8_t rc = (id[2] >> 4) & 0x0f;
        switch (op) {
        default:
            sim->cpu.f[rd] = cpu_do_fpu1(sim, op, ra, rb, rc);
            break;
        }
    }
    case XM_CB_DEBUG: {
        switch (id[0] >> 4) {
        case 0x0f: {
            printf("halted at %8x\n", sim->cpu.pc);
            return CPUE_HALT;
        }
        }
        assert(0);
        break;
    }
    }
    return CPUE_HALT;
}

int main(int argc, char *argv[]) {
    sim_state_t* sim = malloc(sizeof(sim_state_t));
    cpu_execute_result_t cer = CPUE_CONTINUE;
    unsigned long max_ticks = 25;
    sim->cpu.pc = SIM_ROM_BASE;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-quiet")) {
            sim->opt |= SIM_OPT_QUIET;
        } else if (!strcmp(argv[i], "-test")) {
            sim->opt |= SIM_OPT_TEST;
        } else if (!strcmp(argv[i], "-trace-mem")) {
            sim->opt |= SIM_OPT_TRACE_MEM;
        } else if (!strcmp(argv[i], "-t0")) {
            sim->cpu.r[XM_ABI_T0] = SIM_RAM_BASE;
        } else if (!strcmp(argv[i], "-ra")) {
            sim->cpu.r[XM_ABI_RA] = SIM_ROM_BASE;
        } else if (i + 1 < argc && !strcmp(argv[i], "-ticks")) {
            max_ticks = atoll(argv[i + 1]); ++i;
        } else if (i + 1 < argc && !strcmp(argv[i], "-a0")) {
            sim->cpu.r[XM_ABI_A0] = atoll(argv[i + 1]); ++i;
        } else if (i + 1 < argc && !strcmp(argv[i], "-a1")) {
            sim->cpu.r[XM_ABI_A1] = atoll(argv[i + 1]); ++i;
        } else if (i + 1 < argc && !strcmp(argv[i], "-a2")) {
            sim->cpu.r[XM_ABI_A2] = atoll(argv[i + 1]); ++i;
        } else if (i + 1 < argc && !strcmp(argv[i], "-a3")) {
            sim->cpu.r[XM_ABI_A3] = atoll(argv[i + 1]); ++i;
        } else {
            FILE* fp;
            if ((fp = fopen(argv[i], "rb")) != NULL) {
                unsigned long r = fread(sim->rom, 1, sizeof(sim->rom), fp);
                printf("%s: rom read %lu bytes\n", argv[i], r);
                memset(sim->rom + r, 0xff, sizeof(sim->rom) - r);
                if (r < SIM_ROM_SIZE)
                    sim->rom[r] = 0xff;
                fclose(fp);
            }
        }
    }
    
    cpu_debug_print(sim);
    do {
        cer = cpu_step(sim);
        cpu_debug_print(sim);
    } while (cer != CPUE_HALT && sim->perf.ticks < max_ticks);

    free(sim);
    return EXIT_SUCCESS;
}
