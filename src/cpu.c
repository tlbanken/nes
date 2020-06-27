/*
 * cpu.c
 *
 * Travis Banken
 * 2020
 *
 * 6502 cpu implementation for the NES. The NES uses the 2A03/2A07 which
 * behaves the same as the 6502 except it contains the apu for the NES.
 * Sources:
 * http://archive.6502.org/datasheets/rockwell_r650x_r651x.pdf
 * https://wiki.nesdev.com/w/index.php/CPU
 */

#include <utils.h>
#include <cpu.h>
#include <mem.h>

// static function prototypes
#include "_cpu.h"

#define SP (0x0100 | sp)

// interrupt vector locations
#define NMI_VECTOR 0xFFFA
#define RESET_VECTOR 0xFFFC
#define IRQ_VECTOR 0xFFFE

#define LOG(fmt, ...) neslog(LID_CPU, fmt, ##__VA_ARGS__);

// PSR bit field values
typedef enum psr_flags {
    PSR_C  = (1 << 0), // carry
    PSR_Z  = (1 << 1), // zero
    PSR_I  = (1 << 2), // irq disable
    PSR_D  = (1 << 3), // decimal mode (not used for nes)
    PSR_B0 = (1 << 4), // B0 and B1 are fake flags which only exist when psr is
    PSR_B1 = (1 << 5), // pushed onto the stack. They help determine interrupt type
    PSR_V  = (1 << 6), // Overflow
    PSR_N  = (1 << 7)  // Negative
} psr_flags_t;

#define NUM_OPS 256

typedef int(*op_func)();
static op_func opmatrix[NUM_OPS];

typedef struct cpu_state {
    // Registers
    u8 acc;
    u8 x;
    u8 y;
    u8 psr;
    u8 sp;
    u16 pc;

    u32 cycle;
    u8 op;
} cpu_state_t;
static cpu_state_t state;
static cpu_state_t prev_state;

void cpu_init()
{
    // set initial state to RESET state
    cpu_reset();

    // setup opcode matrix
    opmatrix[0x0*16+0x0] = brk;
    opmatrix[0x0*16+0x1] = ora;
    opmatrix[0x0*16+0x2] = undef;
    opmatrix[0x0*16+0x3] = undef;
    opmatrix[0x0*16+0x4] = undef;
    opmatrix[0x0*16+0x5] = ora;
    opmatrix[0x0*16+0x6] = asl;
    opmatrix[0x0*16+0x7] = undef;
    opmatrix[0x0*16+0x8] = php;
    opmatrix[0x0*16+0x9] = ora;
    opmatrix[0x0*16+0xA] = asl;
    opmatrix[0x0*16+0xB] = undef;
    opmatrix[0x0*16+0xC] = undef;
    opmatrix[0x0*16+0xD] = ora;
    opmatrix[0x0*16+0xE] = asl;
    opmatrix[0x0*16+0xF] = undef;
    // MSD 1
    opmatrix[0x1*16+0x0] = bpl;
    opmatrix[0x1*16+0x1] = ora;
    opmatrix[0x1*16+0x2] = undef;
    opmatrix[0x1*16+0x3] = undef;
    opmatrix[0x1*16+0x4] = undef;
    opmatrix[0x1*16+0x5] = ora;
    opmatrix[0x1*16+0x6] = asl;
    opmatrix[0x1*16+0x7] = undef;
    opmatrix[0x1*16+0x8] = clc;
    opmatrix[0x1*16+0x9] = ora;
    opmatrix[0x1*16+0xA] = undef;
    opmatrix[0x1*16+0xB] = undef;
    opmatrix[0x1*16+0xC] = undef;
    opmatrix[0x1*16+0xD] = ora;
    opmatrix[0x1*16+0xE] = asl;
    opmatrix[0x1*16+0xF] = undef;
    // MSD 2
    opmatrix[0x2*16+0x0] = jsr;
    opmatrix[0x2*16+0x1] = and;
    opmatrix[0x2*16+0x2] = undef;
    opmatrix[0x2*16+0x3] = undef;
    opmatrix[0x2*16+0x4] = bit;
    opmatrix[0x2*16+0x5] = and;
    opmatrix[0x2*16+0x6] = rol;
    opmatrix[0x2*16+0x7] = undef;
    opmatrix[0x2*16+0x8] = plp;
    opmatrix[0x2*16+0x9] = and;
    opmatrix[0x2*16+0xA] = rol;
    opmatrix[0x2*16+0xB] = undef;
    opmatrix[0x2*16+0xC] = bit;
    opmatrix[0x2*16+0xD] = and;
    opmatrix[0x2*16+0xE] = rol;
    opmatrix[0x2*16+0xF] = undef;
    // MSD 3
    opmatrix[0x3*16+0x0] = bmi;
    opmatrix[0x3*16+0x1] = and;
    opmatrix[0x3*16+0x2] = undef;
    opmatrix[0x3*16+0x3] = undef;
    opmatrix[0x3*16+0x4] = undef;
    opmatrix[0x3*16+0x5] = and;
    opmatrix[0x3*16+0x6] = rol;
    opmatrix[0x3*16+0x7] = undef;
    opmatrix[0x3*16+0x8] = sec;
    opmatrix[0x3*16+0x9] = and;
    opmatrix[0x3*16+0xA] = undef;
    opmatrix[0x3*16+0xB] = undef;
    opmatrix[0x3*16+0xC] = undef;
    opmatrix[0x3*16+0xD] = and;
    opmatrix[0x3*16+0xE] = rol;
    opmatrix[0x3*16+0xF] = undef;
    // MSD 4
    opmatrix[0x4*16+0x0] = rti;
    opmatrix[0x4*16+0x1] = eor;
    opmatrix[0x4*16+0x2] = undef;
    opmatrix[0x4*16+0x3] = undef;
    opmatrix[0x4*16+0x4] = undef;
    opmatrix[0x4*16+0x5] = eor;
    opmatrix[0x4*16+0x6] = lsr;
    opmatrix[0x4*16+0x7] = undef;
    opmatrix[0x4*16+0x8] = pha;
    opmatrix[0x4*16+0x9] = eor;
    opmatrix[0x4*16+0xA] = lsr;
    opmatrix[0x4*16+0xB] = undef;
    opmatrix[0x4*16+0xC] = jmp;
    opmatrix[0x4*16+0xD] = eor;
    opmatrix[0x4*16+0xE] = lsr;
    opmatrix[0x4*16+0xF] = undef;
    // MSD 5
    opmatrix[0x5*16+0x0] = bvc;
    opmatrix[0x5*16+0x1] = eor;
    opmatrix[0x5*16+0x2] = undef;
    opmatrix[0x5*16+0x3] = undef;
    opmatrix[0x5*16+0x4] = undef;
    opmatrix[0x5*16+0x5] = eor;
    opmatrix[0x5*16+0x6] = lsr;
    opmatrix[0x5*16+0x7] = undef;
    opmatrix[0x5*16+0x8] = cli;
    opmatrix[0x5*16+0x9] = eor;
    opmatrix[0x5*16+0xA] = undef;
    opmatrix[0x5*16+0xB] = undef;
    opmatrix[0x5*16+0xC] = undef;
    opmatrix[0x5*16+0xD] = eor;
    opmatrix[0x5*16+0xE] = lsr;
    opmatrix[0x5*16+0xF] = undef;
    // MSD 6
    opmatrix[0x6*16+0x0] = rts;
    opmatrix[0x6*16+0x1] = adc;
    opmatrix[0x6*16+0x2] = undef;
    opmatrix[0x6*16+0x3] = undef;
    opmatrix[0x6*16+0x4] = undef;
    opmatrix[0x6*16+0x5] = adc;
    opmatrix[0x6*16+0x6] = ror;
    opmatrix[0x6*16+0x7] = undef;
    opmatrix[0x6*16+0x8] = pla;
    opmatrix[0x6*16+0x9] = adc;
    opmatrix[0x6*16+0xA] = ror;
    opmatrix[0x6*16+0xB] = undef;
    opmatrix[0x6*16+0xC] = jmp;
    opmatrix[0x6*16+0xD] = adc;
    opmatrix[0x6*16+0xE] = ror;
    opmatrix[0x6*16+0xF] = undef;
    // MSD 7
    opmatrix[0x7*16+0x0] = bvs;
    opmatrix[0x7*16+0x1] = adc;
    opmatrix[0x7*16+0x2] = undef;
    opmatrix[0x7*16+0x3] = undef;
    opmatrix[0x7*16+0x4] = undef;
    opmatrix[0x7*16+0x5] = adc;
    opmatrix[0x7*16+0x6] = ror;
    opmatrix[0x7*16+0x7] = undef;
    opmatrix[0x7*16+0x8] = sei;
    opmatrix[0x7*16+0x9] = adc;
    opmatrix[0x7*16+0xA] = undef;
    opmatrix[0x7*16+0xB] = undef;
    opmatrix[0x7*16+0xC] = undef;
    opmatrix[0x7*16+0xD] = adc;
    opmatrix[0x7*16+0xE] = ror;
    opmatrix[0x7*16+0xF] = undef;
    // MSD 8
    opmatrix[0x8*16+0x0] = undef;
    opmatrix[0x8*16+0x1] = sta;
    opmatrix[0x8*16+0x2] = undef;
    opmatrix[0x8*16+0x3] = undef;
    opmatrix[0x8*16+0x4] = sty;
    opmatrix[0x8*16+0x5] = sta;
    opmatrix[0x8*16+0x6] = stx;
    opmatrix[0x8*16+0x7] = undef;
    opmatrix[0x8*16+0x8] = dey;
    opmatrix[0x8*16+0x9] = undef;
    opmatrix[0x8*16+0xA] = txa;
    opmatrix[0x8*16+0xB] = undef;
    opmatrix[0x8*16+0xC] = sty;
    opmatrix[0x8*16+0xD] = sta;
    opmatrix[0x8*16+0xE] = stx;
    opmatrix[0x8*16+0xF] = undef;
    // MSD 9
    opmatrix[0x9*16+0x0] = bcc;
    opmatrix[0x9*16+0x1] = sta;
    opmatrix[0x9*16+0x2] = undef;
    opmatrix[0x9*16+0x3] = undef;
    opmatrix[0x9*16+0x4] = sty;
    opmatrix[0x9*16+0x5] = sta;
    opmatrix[0x9*16+0x6] = stx;
    opmatrix[0x9*16+0x7] = undef;
    opmatrix[0x9*16+0x8] = tya;
    opmatrix[0x9*16+0x9] = sta;
    opmatrix[0x9*16+0xA] = txs;
    opmatrix[0x9*16+0xB] = undef;
    opmatrix[0x9*16+0xC] = undef;
    opmatrix[0x9*16+0xD] = sta;
    opmatrix[0x9*16+0xE] = undef;
    opmatrix[0x9*16+0xF] = undef;
    // MSD 10
    opmatrix[0xA*16+0x0] = ldy;
    opmatrix[0xA*16+0x1] = lda;
    opmatrix[0xA*16+0x2] = ldx;
    opmatrix[0xA*16+0x3] = undef;
    opmatrix[0xA*16+0x4] = ldy;
    opmatrix[0xA*16+0x5] = lda;
    opmatrix[0xA*16+0x6] = ldx;
    opmatrix[0xA*16+0x7] = undef;
    opmatrix[0xA*16+0x8] = tay;
    opmatrix[0xA*16+0x9] = lda;
    opmatrix[0xA*16+0xA] = tax;
    opmatrix[0xA*16+0xB] = undef;
    opmatrix[0xA*16+0xC] = ldy;
    opmatrix[0xA*16+0xD] = lda;
    opmatrix[0xA*16+0xE] = ldx;
    opmatrix[0xA*16+0xF] = undef;
    // MSD 11
    opmatrix[0xB*16+0x0] = bcs;
    opmatrix[0xB*16+0x1] = lda;
    opmatrix[0xB*16+0x2] = undef;
    opmatrix[0xB*16+0x3] = undef;
    opmatrix[0xB*16+0x4] = ldy;
    opmatrix[0xB*16+0x5] = lda;
    opmatrix[0xB*16+0x6] = ldx;
    opmatrix[0xB*16+0x7] = undef;
    opmatrix[0xB*16+0x8] = clv;
    opmatrix[0xB*16+0x9] = lda;
    opmatrix[0xB*16+0xA] = tsx;
    opmatrix[0xB*16+0xB] = undef;
    opmatrix[0xB*16+0xC] = ldy;
    opmatrix[0xB*16+0xD] = lda;
    opmatrix[0xB*16+0xE] = ldx;
    opmatrix[0xB*16+0xF] = undef;
    // MSD 12
    opmatrix[0xC*16+0x0] = cpy;
    opmatrix[0xC*16+0x1] = cmp;
    opmatrix[0xC*16+0x2] = undef;
    opmatrix[0xC*16+0x3] = undef;
    opmatrix[0xC*16+0x4] = cpy;
    opmatrix[0xC*16+0x5] = cmp;
    opmatrix[0xC*16+0x6] = dec;
    opmatrix[0xC*16+0x7] = undef;
    opmatrix[0xC*16+0x8] = iny;
    opmatrix[0xC*16+0x9] = cmp;
    opmatrix[0xC*16+0xA] = dex;
    opmatrix[0xC*16+0xB] = undef;
    opmatrix[0xC*16+0xC] = cpy;
    opmatrix[0xC*16+0xD] = cmp;
    opmatrix[0xC*16+0xE] = dec;
    opmatrix[0xC*16+0xF] = undef;
    // MSD 13
    opmatrix[0xD*16+0x0] = bne;
    opmatrix[0xD*16+0x1] = cmp;
    opmatrix[0xD*16+0x2] = undef;
    opmatrix[0xD*16+0x3] = undef;
    opmatrix[0xD*16+0x4] = undef;
    opmatrix[0xD*16+0x5] = cmp;
    opmatrix[0xD*16+0x6] = dec;
    opmatrix[0xD*16+0x7] = undef;
    opmatrix[0xD*16+0x8] = cld;
    opmatrix[0xD*16+0x9] = cmp;
    opmatrix[0xD*16+0xA] = undef;
    opmatrix[0xD*16+0xB] = undef;
    opmatrix[0xD*16+0xC] = undef;
    opmatrix[0xD*16+0xD] = cmp;
    opmatrix[0xD*16+0xE] = dec;
    opmatrix[0xD*16+0xF] = undef;
    // MSD 14
    opmatrix[0xE*16+0x0] = cpx;
    opmatrix[0xE*16+0x1] = sbc;
    opmatrix[0xE*16+0x2] = undef;
    opmatrix[0xE*16+0x3] = undef;
    opmatrix[0xE*16+0x4] = cpx;
    opmatrix[0xE*16+0x5] = sbc;
    opmatrix[0xE*16+0x6] = inc;
    opmatrix[0xE*16+0x7] = undef;
    opmatrix[0xE*16+0x8] = inx;
    opmatrix[0xE*16+0x9] = sbc;
    opmatrix[0xE*16+0xA] = nop;
    opmatrix[0xE*16+0xB] = undef;
    opmatrix[0xE*16+0xC] = cpx;
    opmatrix[0xE*16+0xD] = sbc;
    opmatrix[0xE*16+0xE] = inc;
    opmatrix[0xE*16+0xF] = undef;
    // MSD 15
    opmatrix[0xF*16+0x0] = beq;
    opmatrix[0xF*16+0x1] = sbc;
    opmatrix[0xF*16+0x2] = undef;
    opmatrix[0xF*16+0x3] = undef;
    opmatrix[0xF*16+0x4] = undef;
    opmatrix[0xF*16+0x5] = sbc;
    opmatrix[0xF*16+0x6] = inc;
    opmatrix[0xF*16+0x7] = undef;
    opmatrix[0xF*16+0x8] = sed;
    opmatrix[0xF*16+0x9] = sbc;
    opmatrix[0xF*16+0xA] = undef;
    opmatrix[0xF*16+0xB] = undef;
    opmatrix[0xF*16+0xC] = undef;
    opmatrix[0xF*16+0xD] = sbc;
    opmatrix[0xF*16+0xE] = inc;
    opmatrix[0xF*16+0xF] = undef;
}

int cpu_step()
{
    prev_state = state;
    LOG("%04X ", state.pc);
    // fetch instruction
    // low 4 bits = LSD
    // high 4 bits = MSD
    u8 opcode = cpu_read(state.pc++);
    state.op = opcode;
    int op_index = ((opcode >> 4) & 0xF) * 16 + (opcode & 0xF); 
    // execute instruction
    int clocks = opmatrix[op_index]();
    assert(clocks != 0);
    state.cycle += clocks;
    LOG("\t\tA:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%u (+%d)\n", prev_state.acc,
        prev_state.x, prev_state.y, prev_state.psr, prev_state.sp, prev_state.cycle,
        clocks);
    return clocks;
}

// *** INTERRUPT GENERATORS ***
void cpu_irq()
{
    ERROR("IRQ not supported!\n");
    EXIT(1);
}

void cpu_nmi()
{
    ERROR("NMI not supported!\n");
    EXIT(1);
}

// initial values according to http://wiki.nesdev.com/w/index.php/CPU_power_up_state
void cpu_reset()
{
    u16 lo = cpu_read(RESET_VECTOR);
    u16 hi = cpu_read(RESET_VECTOR + 1);
    state.pc = (hi << 8) | lo;
    state.pc = 0xC000; // NOTE: FOR TESTING
    // state.sp = 0xFF;
    state.sp = 0xFD; // NOTE: FOR TESTING
    // state.psr = 0x34;
    state.psr = 0x24; // NOTE: FOR TESTING
    state.x = 0;
    state.y = 0;
    state.acc = 0;
    // state.cycle = 0;
    state.cycle = 7; // NOTE: FOR TESTING
}

// *** PSR HELPERS ***
static void set_flag(psr_flags_t flag, bool cond)
{
    if (cond) {
        state.psr |= flag;
    } else {
        state.psr &= ~flag;
    }
}

// *** ADDRESS MODE HANDLERS ***
// NOTE: Nothing to be fetched (but we log for consistancy)
static int mode_acc()
{
    LOG("      ");
    LOG(" %s A", op_to_str(state.op));
    return 0;
}

static int mode_imm(u8 *fetch)
{
    *fetch = cpu_read(state.pc++);
    LOG(" %02X   ", *fetch);
    LOG(" %s #$%02X", op_to_str(state.op), *fetch);
    return 0;
}

static int mode_abs(u8 *fetch, u16 *from)
{
    u16 lo = cpu_read(state.pc++);
    u16 hi = cpu_read(state.pc++);
    LOG(" %02X %02X", lo, hi);
    u16 addr = (hi << 8) | lo;

    *fetch = cpu_read(addr);
    *from = addr;
    LOG(" %s $%04X = %02X", addr, *fetch);
    return 0;
}

static int mode_zp(u8 *fetch, u16 *from)
{
    u16 zaddr = cpu_read(state.pc++);
    LOG(" %02X   ", zaddr);

    *fetch = cpu_read(zaddr);
    *from = zaddr;
    LOG(" %s $%02X = %02X", zaddr, *fetch);
    return 0;
}

static int mode_zpx(u8 *fetch, u16 *from)
{
    u16 zaddr = cpu_read(state.pc++);
    LOG(" %02X   ", zaddr);

    *from = (zaddr + state.x) & 0xFF;
    *fetch = cpu_read(*from);
    LOG(" %s $%02X,X @ %02X = %02X", op_to_str(state.op), zaddr, *from, *fetch);
    return 0;
}

static int mode_zpy(u8 *fetch, u16 *from)
{
    u16 zaddr = cpu_read(state.pc++);
    LOG(" %02X   ", zaddr);

    *from = (zaddr + state.y) & 0xFF;
    *fetch = cpu_read(*from);
    LOG(" %s $%02X,Y @ %02X = %02X", op_to_str(state.op), zaddr, *from, *fetch);
    return 0;
}

static int mode_absx(u8 *fetch, u16 *from)
{
    u16 lo = cpu_read(state.pc++);
    u16 hi = cpu_read(state.pc++);
    u16 addr = (hi << 8) | lo;
    LOG(" %02X %02X");

    *from = (addr + state.x);
    *fetch = cpu_read(*from);
    LOG(" %s $%04X,X @ %04X = %02X", op_to_str(state.op), addr, *from, *fetch);
    // check if extra cycle needed (from page cross)
    return (u16)state.x + lo > 0xFF ? 1 : 0;
}

static int mode_absy(u8 *fetch, u16 *from)
{
    u16 lo = cpu_read(state.pc++);
    u16 hi = cpu_read(state.pc++);
    u16 addr = (hi << 8) | lo;
    LOG(" %02X %02X");

    *from = (addr + state.y);
    *fetch = cpu_read(*from);
    LOG(" %s $%04X,Y @ %04X = %02X", op_to_str(state.op), addr, *from, *fetch);
    // check if extra cycle needed (from page cross)
    return (u16)state.y + lo > 0xFF ? 1 : 0;
}

// NOTE: nothing to fetch, but we still need to log
static int mode_imp()
{
    LOG("      ");
    LOG(" %s", op_to_str(state.op));
    return 0;
}

static int mode_rel(u16 *fetch)
{
    u16 rel = cpu_read(state.pc++);
    LOG(" %02X   ", rel);

    // turn rel into a signed number (2's complement)
    u8 carry = (rel & 0x80) >> 7;
    rel = rel & 0x80 ? ~rel : rel;

    *fetch = rel + state.pc + carry;
    LOG(" %s $%04X", *fetch);
    // check if page boundary crossed (bit 8 should be same if no cross)
    return (*fetch ^ state.pc) & 0x0100 ? 1 : 0;
}

static int mode_indx(u8 *fetch, u16 *from)
{
    u16 a = cpu_read(state.pc++);
    u16 ind_addr = (a + state.x) & 0xFF;
    LOG(" %02X   ", a);

    u16 lo = cpu_read(ind_addr);
    u16 hi = cpu_read((ind_addr + 1) & 0xFF);
    u16 addr = (hi << 8) | lo;

    *fetch = cpu_read(addr);
    *from = addr;
    LOG(" %s ($%02X,X) @ %02X = %04X = %02X", op_to_str(state.op), a, ind_addr,
        addr, *fetch);

    return 0;
}

static int mode_indy(u8 *fetch, u16 *from)
{
    u16 ind_addr = cpu_read(state.pc++);
    LOG(" %02X   ", ind_addr);

    u16 lo = cpu_read(ind_addr);
    u16 hi = cpu_read((ind_addr + 1) & 0xFF);

    u16 addr = (hi << 8) | lo;
    u16 yaddr = addr + state.y;

    *fetch = cpu_read(yaddr);
    *from = yaddr;
    LOG(" %s (%02X,Y) = %04X @ %04X = %02X", op_to_str(state.op), ind_addr, addr,
        yaddr, *fetch);

    return (yaddr ^ addr) & 0x0100 ? 1 : 0;
}

static int mode_ind(u16 *fetch)
{
    u16 ind_lo = cpu_read(state.pc++);
    u16 ind_hi = cpu_read(state.pc++);
    LOG(" %02X %02X", ind_lo, ind_hi);

    u16 ind_addr = (ind_hi << 8) | ind_lo;
    u16 lo = cpu_read(ind_addr);
    u16 hi;
    // The 6502 has a bug when the indirect vector falls on a page boundary, the
    // MSB is fetched from $xx00 instead of ($xxFF + 1). aka it wraps around.
    if (ind_lo == 0xFF) {
        hi = cpu_read(ind_addr & 0xFF00);
    } else {
        hi = cpu_read(ind_addr + 1);
    }

    *fetch = (hi << 8) | lo;
    LOG(" %s ($%04X) = %04X", op_to_str(state.op), ind_addr, *fetch);
    return 0;
}


// *** INSTRUCTION HANDLERS ***
static int undef()
{
    return 0;
}

static int adc()
{
    return 0;
}

static int and()
{
    return 0;
}

static int asl()
{
    return 0;
}

static int bcc()
{
    return 0;
}

static int bcs()
{
    return 0;
}

static int beq()
{
    return 0;
}

static int bit()
{
    return 0;
}

static int bmi()
{
    return 0;
}

static int bne()
{
    return 0;
}

static int bpl()
{
    return 0;
}

static int brk()
{
    return 0;
}

static int bvc()
{
    return 0;
}

static int bvs()
{
    return 0;
}

static int clc()
{
    return 0;
}

static int cld()
{
    return 0;
}

static int cli()
{
    return 0;
}

static int clv()
{
    return 0;
}

static int cmp()
{
    return 0;
}

static int cpx()
{
    return 0;
}

static int cpy()
{
    return 0;
}

static int dec()
{
    return 0;
}

static int dex()
{
    return 0;
}

static int dey()
{
    return 0;
}

static int eor()
{
    return 0;
}

static int inc()
{
    return 0;
}

static int inx()
{
    return 0;
}

static int iny()
{
    return 0;
}

static int jmp()
{
    return 0;
}

static int jsr()
{
    return 0;
}

static int lda()
{
    return 0;
}

static int ldx()
{
    return 0;
}

static int ldy()
{
    return 0;
}

static int lsr()
{
    return 0;
}

static int nop()
{
    return 0;
}

static int ora()
{
    return 0;
}

static int pha()
{
    return 0;
}

static int php()
{
    return 0;
}

static int pla()
{
    return 0;
}

static int plp()
{
    return 0;
}

static int rol()
{
    return 0;
}

static int ror()
{
    return 0;
}

static int rti()
{
    return 0;
}

static int rts()
{
    return 0;
}

static int sbc()
{
    return 0;
}

static int sec()
{
    return 0;
}

static int sed()
{
    return 0;
}

static int sei()
{
    return 0;
}

static int sta()
{
    return 0;
}

static int stx()
{
    return 0;
}

static int sty()
{
    return 0;
}

static int tax()
{
    return 0;
}

static int tay()
{
    return 0;
}

static int tsx()
{
    return 0;
}

static int txa()
{
    return 0;
}

static int txs()
{
    return 0;
}

static int tya()
{
    return 0;
}

