#include <complex.h>
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
static float cpu_i_maxf(float a, float b) {
    return a > b ? a : b;
}
static float cpu_i_minf(float a, float b) {
    return a < b ? a : b;
}
static float cpu_i_clampf(float a, float low, float upper) {
    return cpu_i_minf(cpu_i_maxf(a, low), upper);
}

#define CPU_INSTRUCTION_FN(NAME) static cpu_execute_result_t cpu_exec_##NAME(sim_state_t* sim, uint8_t id[])
#define CPU_ALU_UPDATE_FLAGS(VALUE) \
    sim->cpu.flags &= ~(FLAGS_BIT_Z | FLAGS_BIT_N); \
    sim->cpu.flags |= (VALUE) == 0 ? FLAGS_BIT_Z : 0; \
    sim->cpu.flags |= (int32_t)(VALUE) < 0 ? FLAGS_BIT_N : 0;

struct cpu_decode_f4x4 {
    float *dp;
    float v[3];
};
static struct cpu_decode_f4x4 cpu_decode_f4x4(sim_state_t* sim, uint8_t id[]) {
    struct cpu_decode_f4x4 ds;
    ds.dp = &sim->cpu.f[id[1] & 0x0f];
    ds.v[0] = sim->cpu.f[(id[1] >> 4) & 0x0f];
    ds.v[1] = sim->cpu.f[id[2] & 0x0f];
    ds.v[2] = sim->cpu.f[(id[2] >> 4) & 0x0f];
    return ds;
}
CPU_INSTRUCTION_FN(fadd3) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = ds.v[0] + ds.v[1] + ds.v[2];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsub3) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = (ds.v[0] + ds.v[1]) - ds.v[2];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fdiv3) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = (ds.v[0] + ds.v[1]) / ds.v[2];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fmul3) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = (ds.v[0] + ds.v[1]) / ds.v[2];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fmod3) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = fmod(ds.v[0] + ds.v[1], ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fmadd) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = ds.v[0] + ds.v[1] * ds.v[2];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fmsub) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = ds.v[0] - ds.v[1] * ds.v[2];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsqrt3) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = sqrtf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fhyp) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = hypotf(ds.v[0] + ds.v[1], ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fnorm) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = sqrtf(ds.v[0] * ds.v[0] + ds.v[1] * ds.v[1] + ds.v[2] * ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fabs) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = fabs(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsign) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = signbit(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fnabs) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = -fabs(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fcos) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = cosf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsin) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = sinf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(ftan) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = tanf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(facos) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = acosf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fatan) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = atanf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fasin) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = asinf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fcbrt) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = cbrtf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fy0) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = y0f(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fy1) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = y1f(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fj0) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = j0f(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fj1) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = j1f(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fexp) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = expf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(frsqrt) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = 1.f / sqrtf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(frcbrt) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = 1.f / cbrtf (ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fpow2) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = powf(ds.v[0] + ds.v[1], ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fpow3) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = powf(powf(ds.v[0], ds.v[1]), ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fmax) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = cpu_i_maxf(ds.v[0] + ds.v[1], ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fmin) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = cpu_i_minf(ds.v[0] + ds.v[1], ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fclamp) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = cpu_i_clampf(ds.v[0], ds.v[1], ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(finv) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = 1.f / (ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fconstpi) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = M_PI * (ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fconste) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = M_E * (ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fconstpi2) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = M_PI_2 * (ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(frad) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = (ds.v[0] + ds.v[1] + ds.v[2]) * M_PI / 180.f;
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fdeg) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = (ds.v[0] + ds.v[1] + ds.v[2]) * 180.f / M_PI;
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsel) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = (ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsel2) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = (ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fgamma) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = gammaf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(flgamma) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = lgammaf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fround) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = roundf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(ffloor) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = floorf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fceil) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    *ds.dp = ceilf(ds.v[0] + ds.v[1] + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(faddcrr) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = creal(c + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsubcrr) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = creal(c - ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fdivcrr) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = creal(c / ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fmulcrr) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = creal(c * ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsqrtcrr) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = creal(csqrtf(c) * ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(faddcri) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = cimag(c + ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsubcri) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = cimag(c - ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fdivcri) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = cimag(c / ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fmulcri) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = cimag(c * ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fsqrtcri) {
    struct cpu_decode_f4x4 ds = cpu_decode_f4x4(sim, id);
    float complex c = ds.v[0] + ds.v[1] * I;
    *ds.dp = cimag(csqrtf(c) * ds.v[2]);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
struct cpu_decode_r4f4x3 {
    uint8_t rd, ra, rb, rc;
};
static struct cpu_decode_r4f4x3 cpu_decode_r4f4x3(sim_state_t* sim, uint8_t id[]) {
    struct cpu_decode_r4f4x3 ds;
    ds.rd = id[1] & 0x0f;
    ds.ra = (id[1] >> 4) & 0x0f;
    ds.rb = id[2] & 0x0f;
    ds.rc = (id[2] >> 4) & 0x0f;
    return ds;
}
CPU_INSTRUCTION_FN(fcvti) {
    struct cpu_decode_r4f4x3 ds = cpu_decode_r4f4x3(sim, id);
    *(float*)(&sim->cpu.r[ds.rd]) = sim->cpu.f[ds.ra] + sim->cpu.f[ds.rb] + sim->cpu.f[ds.rc];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(icvtf) {
    struct cpu_decode_r4f4x3 ds = cpu_decode_r4f4x3(sim, id);
    *(uint32_t*)(&sim->cpu.f[ds.rd]) = sim->cpu.r[ds.ra] + sim->cpu.r[ds.rb] + sim->cpu.r[ds.rc];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(fcvtri) {
    struct cpu_decode_r4f4x3 ds = cpu_decode_r4f4x3(sim, id);
    sim->cpu.r[ds.rd] = sim->cpu.f[ds.ra] + sim->cpu.f[ds.rb] + sim->cpu.f[ds.rc];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(icvtrf) {
    struct cpu_decode_r4f4x3 ds = cpu_decode_r4f4x3(sim, id);
    sim->cpu.f[ds.rd] = sim->cpu.r[ds.ra] + sim->cpu.r[ds.rb] + sim->cpu.r[ds.rc];
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
struct cpu_decode_r4x2i8_ifhbs {
    uint32_t *dp;
    uint32_t addr;
    uint32_t a;
    uint32_t b;
};
static struct cpu_decode_r4x2i8_ifhbs cpu_decode_r4x2i8_ifhbs(sim_state_t* sim, uint8_t id[]) {
    struct cpu_decode_r4x2i8_ifhbs ds;
    if ((id[3] & 0x80) != 0) {
        uint8_t rd = id[1] & 0x0f;
        uint8_t ra = (id[1] >> 4) & 0x0f;
        uint8_t imm = id[2];
        ds.addr = sim->cpu.r[ra] + imm * 4;
        ds.dp = &sim->cpu.r[rd];
        ds.a = sim->cpu.r[ra];
        ds.b = imm;
    } else {
        uint8_t rd = id[1] & 0x0f;
        uint8_t ra = (id[1] >> 4) & 0x0f;
        uint8_t rb = id[2] & 0x0f;
        uint8_t imm = (id[2] >> 4) & 0x0f;
        ds.addr = sim->cpu.r[ra] + sim->cpu.r[rb] * imm * 4;
        ds.dp = &sim->cpu.r[rd];
        ds.a = sim->cpu.r[ra];
        ds.b = sim->cpu.r[rb] + imm;
    }
    return ds;
}
CPU_INSTRUCTION_FN(add) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a + ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(sub) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a - ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(mul) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a * ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(div) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a / ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(rem) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a % ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(imul) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = (int32_t)ds.a * (int32_t)ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(and) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a & ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(xor) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a ^ ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(or) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a | ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(shl) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a << ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(shr) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.a >> ds.b;
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(pcnt) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = cpu_i_popcount(ds.a + ds.b);
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(clz) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = cpu_i_clz(ds.a + ds.b);
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(clo) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = cpu_i_clo(ds.a + ds.b);
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(bswap) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = cpu_i_bswap(ds.a + ds.b);
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(ipcnt) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = 32 - cpu_i_popcount(ds.a + ds.b);
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(stb) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    cpu_write8(sim, ds.addr, *ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(stw) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    cpu_write16(sim, ds.addr, *ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(stl) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    cpu_write32(sim, ds.addr, *ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(stq) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    cpu_write32(sim, ds.addr, *ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(ldb) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = cpu_read8(sim, ds.addr);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(ldw) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = cpu_read16(sim, ds.addr);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(ldl) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = cpu_read32(sim, ds.addr);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(ldq) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = cpu_read32(sim, ds.addr);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(lea) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    *ds.dp = ds.addr;
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(mcopy) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(cmp) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    uint32_t r = cpu_i_add32(sim, ds.a, ds.b);
    sim->cpu.flags &= ~(FLAGS_BIT_Z | FLAGS_BIT_N);
    sim->cpu.flags |= r == 0 ? FLAGS_BIT_Z : 0;
    sim->cpu.flags |= (int32_t)r < 0 ? FLAGS_BIT_N : 0;
    *ds.dp = sim->cpu.flags;
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
} 
CPU_INSTRUCTION_FN(cmpkp) {
    struct cpu_decode_r4x2i8_ifhbs ds = cpu_decode_r4x2i8_ifhbs(sim, id);
    uint32_t old_flags = sim->cpu.flags;
    uint32_t r = cpu_i_add32(sim, ds.a, ds.b);
    sim->cpu.flags &= ~(FLAGS_BIT_Z | FLAGS_BIT_N);
    sim->cpu.flags |= r == 0 ? FLAGS_BIT_Z : 0;
    sim->cpu.flags |= (int32_t)r < 0 ? FLAGS_BIT_N : 0;
    *ds.dp = sim->cpu.flags;
    sim->cpu.flags = old_flags; /* restore flags */
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
struct cpu_decode_r4x4 {
    uint32_t *dp;
    uint32_t a;
    uint32_t b;
    uint32_t c;
};
static struct cpu_decode_r4x4 cpu_decode_r4x4(sim_state_t* sim, uint8_t id[]) {
    struct cpu_decode_r4x4 ds;
    uint8_t rd = id[1] & 0x0f;
    uint8_t ra = (id[1] >> 4) & 0x0f;
    uint8_t rb = id[2] & 0x0f;
    uint8_t rc = (id[2] >> 4) & 0x0f;
    ds.dp = &sim->cpu.r[rd];
    ds.a = sim->cpu.r[ra];
    ds.b = sim->cpu.r[rb];
    ds.c = sim->cpu.r[rc];
    return ds;
}
CPU_INSTRUCTION_FN(memcpy) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    for (size_t i = 0; i < ds.c; ++i)
        cpu_write8(sim, ds.a + i, cpu_read8(sim, ds.b + i));
    *ds.dp = ds.a;
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(memmov) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    if (ds.a > ds.b) for (size_t i = 0; i < ds.c; ++i)
        cpu_write8(sim, ds.a + i, cpu_read8(sim, ds.b + i));
    else for (size_t i = 0; i < ds.c; ++i)
        cpu_write8(sim, ds.a + ds.c - i, cpu_read8(sim, ds.b + ds.c - i));
    *ds.dp = ds.a;
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(memset) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    for (size_t i = 0; i < ds.c; ++i)
        cpu_write8(sim, ds.a + i, ds.b);
    *ds.dp = ds.a;
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(memchr) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    *ds.dp = 0; /* Null */
    for (size_t i = 0; i < ds.c; ++i)
        if (cpu_read8(sim, ds.a + i) == ds.b) {
            *ds.dp = ds.a;
            break;
        }
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(memchrf) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    *ds.dp = 0; /* Null */
    for (size_t i = 0; i < ds.c; ++i)
        if (cpu_read8(sim, ds.a + i) == ds.b) {
            *ds.dp = ds.a;
            break;
        }
    CPU_ALU_UPDATE_FLAGS(*ds.dp);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(strcpy) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    size_t i = 0;
    char c;
    do {
        c = cpu_read8(sim, ds.b + i);
        cpu_write8(sim, ds.a + i, c);
        if (c == '\0')
            break;
        ++i;
    } while (c != '\0');
    *ds.dp = ds.a;
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(strcat) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    /* TODO */ abort();
    *ds.dp = ds.a;
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(strpbrk) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    /* TODO */ abort();
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(strncpy) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    *ds.dp = ds.a;
    for (size_t i = 0; i < ds.c; ++i) {
        char c = cpu_read8(sim, ds.b + i);
        cpu_write8(sim, ds.a + i, c);
        if (c == '\0')
            break;
    }
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(strncat) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    /* TODO */ abort();
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(strchr) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    /* TODO */ abort();
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(strnchr) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    /* TODO */ abort();
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(indtab) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    *ds.dp = cpu_read32(sim, ds.a + (ds.b + ds.c) * sizeof(uint32_t));
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(indtab8) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    *ds.dp = cpu_read32(sim, ds.a + (ds.b + ds.c) * sizeof(uint64_t));
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(chtree) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    uint32_t counter = ds.c;
    uint32_t p = ds.a;
    do p = cpu_read32(sim, p + ds.b); while (p && counter-- > 0);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(chtreeunchk) {
    struct cpu_decode_r4x4 ds = cpu_decode_r4x4(sim, id);
    uint32_t counter = ds.c;
    uint32_t p = ds.a;
    do p = cpu_read32(sim, p + ds.b); while (counter-- > 0);
    sim->cpu.pc += 4;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(jmp) {
    sim->cpu.pc = (((uint32_t)id[1]) << 8) | id[2];
    ++sim->perf.jumps;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(jmprel) {
    int32_t rela = (int32_t)(int16_t)((((uint16_t)id[1]) << 8) | id[2]);
    sim->cpu.pc += rela;
    ++sim->perf.jumps;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(call) {
    uint8_t ra = id[1];
    int32_t rela = (int32_t)(int8_t)id[2];
    sim->cpu.pc = sim->cpu.r[ra] + rela;
    sim->cpu.r[XM_ABI_RA] = sim->cpu.pc + 4;
    ++sim->perf.jumps;
    return CPUE_CONTINUE;
}
CPU_INSTRUCTION_FN(ret) {
    sim->cpu.pc = sim->cpu.r[XM_ABI_RA];
    ++sim->perf.jumps;
    return CPUE_CONTINUE;
}
static cpu_execute_result_t cpu_exec_common_b(sim_state_t *sim, uint8_t id[]) {
    uint8_t ra = id[1] & 0x0f;
    uint8_t cc = (id[1] >> 4);
    int32_t rela = (int32_t)(int8_t)id[2];
    bool cond = 0;
    switch ((id[3] - 0x50) & 0x0f) {
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
}
CPU_INSTRUCTION_FN(bz) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(b) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bgzs) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bgpc) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bgpcrela) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bo) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bgoz) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bemax) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bet0) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bet1) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bet2) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bet3) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bet4) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bet5) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bet6) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(bet7) { return cpu_exec_common_b(sim, id); }
CPU_INSTRUCTION_FN(halt) {
    printf("halted at %8x\n", sim->cpu.pc);
    return CPUE_HALT;
}

static bool cpu_match_inst(sim_state_t* sim, uint8_t id[], enum xm_inst_format format, uint8_t op) {
    enum xm_cb0 cb0 = xm_get_cb0_from_format(format);
    switch (cb0) {
    case XM_CB_INTEGER:
    case XM_CB_FLOAT:
    case XM_CB_CONTROL:
    case XM_CB_TILE:
        return (format == XM_FORMAT_R4R4I8O8_IFHBS && (id[3] & 0x7f) == op)
            || (format != XM_FORMAT_R4R4I8O8_IFHBS && id[3] == op);
    case XM_CB_DEBUG:
        return (id[0] >> 4) == op;
    default:
        abort();
    }
}

cpu_execute_result_t cpu_step(sim_state_t* sim) {
    uint8_t id[8]; /* Instruction ds */

    ++sim->perf.ticks;

    id[0] = cpu_read8(sim, sim->cpu.pc);
    id[1] = cpu_read8(sim, sim->cpu.pc + 1);
    id[2] = cpu_read8(sim, sim->cpu.pc + 2);
    id[3] = cpu_read8(sim, sim->cpu.pc + 3);

#define XM_INST_ELEM(NAME, FORMAT, OP) \
    else if (cpu_match_inst(sim, id, FORMAT, OP)) { \
        printf(" --> " #NAME "\n"); \
        return cpu_exec_##NAME(sim, id); \
    }

    if (0) {}
    XM_INST_LIST
    else {

    }
#undef XM_INST_ELEM
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
