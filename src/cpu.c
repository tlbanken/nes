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
 * http://obelisk.me.uk/6502/reference.html
 */

#include <utils.h>
#include <cpu.h>
#include <mem.h>

// static function prototypes
#include "_cpu.h"

#define SP (0x0100 | state.sp)

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
    // MSD 0
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
    LOG("%02X ", state.op);
    int op_index = ((opcode >> 4) & 0xF) * 16 + (opcode & 0xF); 
    // execute instruction
    int clocks = opmatrix[op_index]();
    assert(clocks != 0);
    state.cycle += clocks;
    LOG("A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%u (+%d)\n", prev_state.acc,
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
static int mode_acc(u8 *fetch)
{
    LOG("      ");
    LOG(" %s A                              ", op_to_str(state.op));
    *fetch = state.acc;
    return 0;
}

static int mode_imm(u8 *fetch)
{
    *fetch = cpu_read(state.pc++);
    LOG(" %02X   ", *fetch);
    LOG(" %s #$%02X                         ", op_to_str(state.op), *fetch);
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
    LOG(" %s $%04X = %02X                   ", addr, *fetch);
    return 0;
}

static int mode_zp(u8 *fetch, u16 *from)
{
    u16 zaddr = cpu_read(state.pc++);
    LOG(" %02X   ", zaddr);

    *fetch = cpu_read(zaddr);
    *from = zaddr;
    LOG(" %s $%02X = %02X                   ", zaddr, *fetch);
    return 0;
}

static int mode_zpx(u8 *fetch, u16 *from)
{
    u16 zaddr = cpu_read(state.pc++);
    LOG(" %02X   ", zaddr);

    *from = (zaddr + state.x) & 0xFF;
    *fetch = cpu_read(*from);
    LOG(" %s $%02X,X @ %02X = %02X          ", op_to_str(state.op), zaddr, *from, *fetch);
    return 0;
}

static int mode_zpy(u8 *fetch, u16 *from)
{
    u16 zaddr = cpu_read(state.pc++);
    LOG(" %02X   ", zaddr);

    *from = (zaddr + state.y) & 0xFF;
    *fetch = cpu_read(*from);
    LOG(" %s $%02X,Y @ %02X = %02X          ", op_to_str(state.op), zaddr, *from, *fetch);
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
    LOG(" %s $%04X,X @ %04X = %02X          ", op_to_str(state.op), addr, *from, *fetch);
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
    LOG(" %s $%04X,Y @ %04X = %02X          ", op_to_str(state.op), addr, *from, *fetch);
    // check if extra cycle needed (from page cross)
    return (u16)state.y + lo > 0xFF ? 1 : 0;
}

// NOTE: nothing to fetch, but we still need to log
static int mode_imp()
{
    LOG("      ");
    LOG(" %s                                ", op_to_str(state.op));
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
    LOG(" %s $%04X                          ", *fetch);
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
    LOG(" %s ($%02X,X) @ %02X = %04X = %02X ", op_to_str(state.op), a, ind_addr,
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
    LOG(" %s (%02X,Y) = %04X @ %04X = %02X  ", op_to_str(state.op), ind_addr, addr,
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
    LOG(" %s ($%04X) = %04X                 ", op_to_str(state.op), ind_addr, *fetch);
    return 0;
}


// *** INSTRUCTION HANDLERS ***
static int undef()
{
    ERROR("Unofficial opcode (%02X) not implementated!\n", state.op);
    EXIT(1);
    return 0;
}

/*
 * ADC - Add with carry
 * Size: 2-3
 * Cycles: 2-6
 * Flags: C, Z, V, N
 */
static int adc()
{
    int clocks = 0;
    u8 val;
    u16 dummy;
    int extra_clock;
    switch (state.op) {
    case 0x69: // IMM -- 2 bytes, 2 cycles
        mode_imm(&val);
        clocks = 2;
        break;
    case 0x65: // ZP -- 2 bytes, 3 cycles
        mode_zp(&val, &dummy);
        clocks = 3;
        break;
    case 0x75: // ZPX -- 2 bytes, 4 cycles
        mode_zpx(&val, &dummy);
        clocks = 4;
        break;
    case 0x6D: // ABS -- 3 bytes, 4 cycles
        mode_abs(&val, &dummy);
        clocks = 4;
        break;
    case 0x7D: // ABSX -- 3 bytes, 4 (+1) cycles
        extra_clock = mode_absx(&val, &dummy);
        clocks = 4 + extra_clock;
        break;
    case 0x79: // ABSY -- 3 bytes, 4 (+1) cycles
        extra_clock = mode_absy(&val, &dummy);
        clocks = 4 + extra_clock;
        break;
    case 0x61: // INDX -- 2 bytes, 6 cycles
        mode_indx(&val, &dummy);
        clocks = 6;
        break;
    case 0x71: // INDY -- 2 bytes, 5 (+1) cycles
        extra_clock = mode_indy(&val, &dummy);
        clocks = 5 + extra_clock;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // add val to acc with carry
    u16 res = (u16)val + (u16)state.acc + (u16)(state.psr & 0x1);
    state.acc = res & 0xFF;

    // set the flags
    set_flag(PSR_C, res & 0x100);
    set_flag(PSR_Z, state.acc == 0);
    set_flag(PSR_V, ~(val ^ prev_state.acc) & (val & state.acc) & 0x80);
    set_flag(PSR_N, state.acc & 0x80);

    return clocks;
}

/*
 * AND - Logical AND
 * Size: 2-3
 * Cycles: 2-6
 * Flags: Z, N
 */
static int and()
{
    int clocks = 0;
    u8 val;
    u16 dummy;
    int extra_clock;
    switch (state.op) {
    case 0x29: // IMM -- 2 bytes, 2 cycles
        mode_imm(&val);
        clocks = 2;
        break;
    case 0x25: // ZP -- 2 bytes, 3 cycles
        mode_zp(&val, &dummy);
        clocks = 3;
        break;
    case 0x35: // ZPX -- 2 bytes, 4 cycles
        mode_zpx(&val, &dummy);
        clocks = 4;
        break;
    case 0x2D: // ABS -- 3 bytes, 4 cycles
        mode_abs(&val, &dummy);
        clocks = 4;
        break;
    case 0x3D: // ABSX -- 3 bytes, 4 (+1) cycles
        extra_clock = mode_absx(&val, &dummy);
        clocks = 4 + extra_clock;
        break;
    case 0x39: // ABSY -- 3 bytes, 4 (+1) cycles
        extra_clock = mode_absy(&val, &dummy);
        clocks = 4 + extra_clock;
        break;
    case 0x21: // INDX -- 2 bytes, 6 cycles
        mode_indx(&val, &dummy);
        clocks = 6;
        break;
    case 0x31: // INDY -- 2 bytes, 5 (+1) cycles
        extra_clock = mode_indy(&val, &dummy);
        clocks = 5 + extra_clock;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // and it up
    state.acc &= val;

    // set flags
    set_flag(PSR_Z, state.acc == 0);
    set_flag(PSR_N, state.acc & 0x80);

    return clocks;
}

/*
 * ASL - Arithmetic Shift Left
 * Size: 1-3
 * Cycles: 2-7
 * Flags: C, Z, N
 */
static int asl()
{
    int clocks = 0;
    u16 from;
    u8 val;
    bool inmem = true;
    switch (state.op) {
    case 0x0A: // ACC -- 1 byte, 2 cycles
        mode_acc(&val);
        clocks = 2;
        inmem = false;
        break;
    case 0x06: // ZP -- 2 bytes, 5 cycles
        mode_zp(&val, &from);
        clocks = 5;
        break;
    case 0x16: // ZPX -- 2 bytes, 6 cycles
        mode_zpx(&val, &from);
        clocks = 6;
        break;
    case 0x0E: // ABS -- 3 bytes, 6 cycles
        mode_abs(&val, &from);
        clocks = 6;
        break;
    case 0x1E: // ABSX -- 3 bytes, 7 cycles
        mode_absx(&val, &from);
        clocks = 7;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
        break;
    }

    // shift left
    u16 res = val << 1;
    if (inmem) {
        cpu_write(res & 0xFF, from);
    } else {
        state.acc = res & 0xFF;
    }

    // set flags
    set_flag(PSR_C, val & 0x80);
    set_flag(PSR_Z, state.acc == 0); // Just acc or mem too???
    set_flag(PSR_N, res & 0x80);

    return clocks;
}

/*
 * BCC - Branch if carry clear
 * Size: 2
 * Cycles: 2-4
 * Flags: None
 */
static int bcc()
{
    assert(state.op == 0x90);
    int clocks = 2;
    u16 baddr;
    int new_page = mode_rel(&baddr);
    if (!(state.psr & PSR_C)) {
        clocks += 1 + new_page;
        state.pc = baddr;
    } 
    return clocks;
}

/*
 * BCS - Branch if carry set
 * Size: 2
 * Cycles: 2-4
 * Flags: None
 */
static int bcs()
{
    assert(state.op == 0xB0);
    int clocks = 2;
    u16 baddr;
    int new_page = mode_rel(&baddr);
    if (state.psr & PSR_C) {
        clocks += 1 + new_page;
        state.pc = baddr;
    } 
    return clocks;
}

/*
 * BEQ - Branch if zero set
 * Size: 2
 * Cycles: 2-4
 * Flags: None
 */
static int beq()
{
    assert(state.op == 0xF0);
    int clocks = 2;
    u16 baddr;
    int new_page = mode_rel(&baddr);
    if (state.psr & PSR_Z) {
        clocks += 1 + new_page;
        state.pc = baddr;
    } 
    return clocks;
}

/*
 * BIT - Bit Test
 * Size: 2-3
 * Cycles: 3-4
 * Flags: Z, V, N
 */
static int bit()
{
    int clocks = 0;
    u16 from;
    u8 val;
    switch (state.op) {
    case 0x24: // ZP -- bytes 2, cycles 3
        mode_zp(&val, &from);
        clocks = 3;
        break;
    case 0x2C: // ABS -- bytes 2, cycles 3
        mode_abs(&val, &from);
        clocks = 3;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // mask
    u8 res = state.acc & val;

    // set flags
    set_flag(PSR_Z, res == 0);
    set_flag(PSR_V, val & 0x40);
    set_flag(PSR_N, val & 0x80);

    return clocks;
}

/*
 * BMI - Branch if Minus
 * Size: 2
 * Cycles: 2-4
 * Flags: None
 */
static int bmi()
{
    assert(state.op == 0x30);
    int clocks = 2;
    u16 baddr;
    int new_page = mode_rel(&baddr);
    if (state.psr & PSR_N) {
        clocks += 1 + new_page;
        state.pc = baddr;
    } 
    return clocks;
}

/*
 * BNE - Branch if not equal
 * Size: 2
 * Cycles: 2-4
 * Flags: None
 */
static int bne()
{
    assert(state.op == 0xD0);
    int clocks = 2;
    u16 baddr;
    int new_page = mode_rel(&baddr);
    if (!(state.psr & PSR_Z)) {
        clocks += 1 + new_page;
        state.pc = baddr;
    } 
    return clocks;
}

/*
 * BPL - Branch if positive
 * Size: 2
 * Cycles: 2-4
 * Flags: None
 */
static int bpl()
{
    assert(state.op == 0x10);
    int clocks = 2;
    u16 baddr;
    int new_page = mode_rel(&baddr);
    if (!(state.psr & PSR_N)) {
        clocks += 1 + new_page;
        state.pc = baddr;
    } 
    return clocks;
}

/*
 * BRK - Force Interrupt
 * Size: 1
 * Cycles: 7
 * Flags: B0/B1 (on stack), I
 */
static int brk()
{
    assert(state.op == 0x00);
    mode_imp();
    // push pc
    u8 hi = state.pc >> 8;
    u8 lo = state.pc & 0xFF;
    cpu_write(hi, SP);
    state.sp--;
    cpu_write(lo, SP);
    state.sp--;
    // push psr
    u8 psr_push = state.psr | PSR_B0 | PSR_B1;
    cpu_write(psr_push, SP);
    state.sp--;

    // set I flag (not sure if needs to be done before stack push)
    set_flag(PSR_I, true);

    // set pc to IRQ interrupt vector
    lo = cpu_read(IRQ_VECTOR);
    hi = cpu_read(IRQ_VECTOR + 1);
    state.pc = (hi << 8) | lo;

    return 7;
}

/*
 * BVC - Branch if Overflow Clear
 * Size: 2
 * Cycles: 2-4
 * Flags: None
 */
static int bvc()
{
    assert(state.op == 0x50);
    int clocks = 2;
    u16 baddr;
    int new_page = mode_rel(&baddr);
    if (!(state.psr & PSR_V)) {
        clocks += 1 + new_page;
        state.pc = baddr;
    } 
    return clocks;
}

/*
 * BVS - Branch if Overflow set
 * Size: 2
 * Cycles: 2-4
 * Flags: None
 */
static int bvs()
{
    assert(state.op == 0x70);
    int clocks = 2;
    u16 baddr;
    int new_page = mode_rel(&baddr);
    if (state.psr & PSR_V) {
        clocks += 1 + new_page;
        state.pc = baddr;
    } 
    return clocks;
}

/*
 * CLC - Clear Carry Flag
 * Size: 1
 * Cycles: 2
 * Flags: C
 */
static int clc()
{
    assert(state.op == 0x18);
    mode_imp();
    set_flag(PSR_C, false);
    return 2;
}

/*
 * CLD - Clear Decimal Flag
 * Size: 1
 * Cycles: 2
 * Flags: D
 */
static int cld()
{
    assert(state.op == 0xD8);
    mode_imp();
    set_flag(PSR_D, false);
    return 2;
}

/*
 * CLI - Clear Interrupt Disable Flag
 * Size: 1
 * Cycles: 2
 * Flags: I
 */
static int cli()
{
    assert(state.op == 0x58);
    mode_imp();
    set_flag(PSR_I, false);
    return 2;
}

/*
 * CLV - Clear Overflow Flag
 * Size: 1
 * Cycles: 2
 * Flags: V
 */
static int clv()
{
    assert(state.op == 0xB8);
    mode_imp();
    set_flag(PSR_V, false);
    return 2;
}

/*
 * CMP - Compare
 * Size: 2-3
 * Cycles: 2-6
 * Flags: C, Z, N
 */
static int cmp()
{
    int clocks = 0;
    u8 val;
    u16 dummy;
    int extra_clock;
    switch (state.op) {
    case 0xC9: // IMM -- 2 bytes, 2 cycles
        mode_imm(&val);
        clocks = 2;
        break;
    case 0xC5: // ZP -- 2 bytes, 3 cycles
        mode_zp(&val, &dummy);
        clocks = 3;
        break;
    case 0xD5: // ZPX -- 2 bytes, 4 cycles
        mode_zpx(&val, &dummy);
        clocks = 4;
        break;
    case 0xCD: // ABS -- 3 bytes, 4 cycles
        mode_abs(&val, &dummy);
        clocks = 4;
        break;
    case 0xDD: // ABSX -- 3 bytes, 4 (+1) cycles
        extra_clock = mode_absx(&val, &dummy);
        clocks = 4 + extra_clock;
        break;
    case 0xD9: // ABSY -- 3 bytes, 4 (+1) cycles
        extra_clock = mode_absy(&val, &dummy);
        clocks = 4 + extra_clock;
        break;
    case 0xC1: // INDX -- 2 bytes, 6 cycles
        mode_indx(&val, &dummy);
        clocks = 6;
        break;
    case 0xD1: // INDY -- 2 bytes, 5 (+1) cycles
        extra_clock = mode_indy(&val, &dummy);
        clocks = 5 + extra_clock;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // compare (using subtraction)
    u8 res = state.acc - val;

    // set flags
    set_flag(PSR_C, state.acc >= val);
    set_flag(PSR_Z, state.acc == val);
    set_flag(PSR_N, res & 0x80);

    return clocks;
}

/*
 * CPX - Compare X register
 * Size: 2-3
 * Cycles: 2-4
 * Flags: C, Z, N
 */
static int cpx()
{
    int clocks = 0;
    u16 from;
    u8 val;
    switch (state.op) {
    case 0xE0: // IMM -- 2 bytes, 2 cycles
        mode_imm(&val);
        clocks = 2;
        break;
    case 0xE4: // ZP -- 2 bytes, 3 cycles
        mode_zp(&val, &from);
        clocks = 3;
        break;
    case 0xEC: // ABS -- 3 bytes, 4 cycles
        mode_abs(&val, &from);
        clocks = 4;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // compare using sub
    u8 res = state.x - val;

    // set flags
    set_flag(PSR_C, state.x >= val);
    set_flag(PSR_Z, state.x == val);
    set_flag(PSR_N, res & 0x80);

    return clocks;
}

/*
 * CPY - Compare Y register
 * Size: 2-3
 * Cycles: 2-4
 * Flags: C, Z, N
 */
static int cpy()
{
    int clocks = 0;
    u16 from;
    u8 val;
    switch (state.op) {
    case 0xC0: // IMM -- 2 bytes, 2 cycles
        mode_imm(&val);
        clocks = 2;
        break;
    case 0xC4: // ZP -- 2 bytes, 3 cycles
        mode_zp(&val, &from);
        clocks = 3;
        break;
    case 0xCC: // ABS -- 3 bytes, 4 cycles
        mode_abs(&val, &from);
        clocks = 4;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // compare using sub
    u8 res = state.y - val;

    // set flags
    set_flag(PSR_C, state.y >= val);
    set_flag(PSR_Z, state.y == val);
    set_flag(PSR_N, res & 0x80);

    return clocks;
}

/*
 * DEC - Decrement Memory
 * Size: 2-3
 * Cycles: 5-7
 * Flags: Z, N
 */
static int dec()
{
    int clocks = 0;
    u16 from;
    u8 val;
    switch (state.op) {
    case 0xC6: // ZP -- 2 bytes, 5 cycles
        mode_zp(&val, &from);
        clocks = 5;
        break;
    case 0xD6: // ZPX -- 2 bytes, 6 cycles
        mode_zpx(&val, &from);
        clocks = 6;
        break;
    case 0xCE: // ABS -- 3 bytes, 6 cycles
        mode_abs(&val, &from);
        clocks = 6;
        break;
    case 0xDE: // ABSX -- 3 bytes, 7 cycles
        mode_absx(&val, &from);
        clocks = 7;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // decrement and store
    u8 res = val - 1;
    cpu_write(res, from);

    // set flags
    set_flag(PSR_Z, res == 0);
    set_flag(PSR_N, res & 0x80);

    return clocks;
}

/*
 * DEX - Decrement X Register
 * Size: 1
 * Cycles: 2
 * Flags: Z, N
 */
static int dex()
{
    assert(state.op == 0xCA);
    mode_imp();
    state.x--;
    // set flags
    set_flag(PSR_Z, state.x == 0);
    set_flag(PSR_N, state.x & 0x80);
    return 2;
}

/*
 * DEY - Decrement Y Register
 * Size: 1
 * Cycles: 2
 * Flags: Z, N
 */
static int dey()
{
    assert(state.op == 0x88);
    mode_imp();
    state.y--;
    // set flags
    set_flag(PSR_Z, state.y == 0);
    set_flag(PSR_N, state.y & 0x80);
    return 2;
}

/*
 * EOR - Exclusive OR
 * Size: 2-3
 * Cycles: 2-6
 * Flags: Z, N
 */
static int eor()
{
    int clocks = 0;
    u8 val;
    u16 dummy;
    int extra_clock;
    switch (state.op) {
    case 0x49: // IMM -- 2 bytes, 2 cycles
        mode_imm(&val);
        clocks = 2;
        break;
    case 0x45: // ZP -- 2 bytes, 3 cycles
        mode_zp(&val, &dummy);
        clocks = 3;
        break;
    case 0x55: // ZPX -- 2 bytes, 4 cycles
        mode_zpx(&val, &dummy);
        clocks = 4;
        break;
    case 0x4D: // ABS -- 3 bytes, 4 cycles
        mode_abs(&val, &dummy);
        clocks = 4;
        break;
    case 0x5D: // ABSX -- 3 bytes, 4 (+1) cycles
        extra_clock = mode_absx(&val, &dummy);
        clocks = 4 + extra_clock;
        break;
    case 0x59: // ABSY -- 3 bytes, 4 (+1) cycles
        extra_clock = mode_absy(&val, &dummy);
        clocks = 4 + extra_clock;
        break;
    case 0x41: // INDX -- 2 bytes, 6 cycles
        mode_indx(&val, &dummy);
        clocks = 6;
        break;
    case 0x51: // INDY -- 2 bytes, 5 (+1) cycles
        extra_clock = mode_indy(&val, &dummy);
        clocks = 5 + extra_clock;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // XOR
    state.acc ^= val;

    // set flags
    set_flag(PSR_Z, state.acc == 0);
    set_flag(PSR_N, state.acc & 0x80);

    return clocks;
}

/*
 * INC - Increment Memory
 * Size: 2-3
 * Cycles: 5-7
 * Flags: Z, N
 */
static int inc()
{
    int clocks = 0;
    u16 from;
    u8 val;
    switch (state.op) {
    case 0xE6: // ZP -- 2 bytes, 5 cycles
        mode_zp(&val, &from);
        clocks = 5;
        break;
    case 0xF6: // ZPX -- 2 bytes, 6 cycles
        mode_zpx(&val, &from);
        clocks = 6;
        break;
    case 0xEE: // ABS -- 3 bytes, 6 cycles
        mode_abs(&val, &from);
        clocks = 6;
        break;
    case 0xFE: // ABSX -- 3 bytes, 7 cycles
        mode_absx(&val, &from);
        clocks = 7;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // decrement and store
    u8 res = val + 1;
    cpu_write(res, from);

    // set flags
    set_flag(PSR_Z, res == 0);
    set_flag(PSR_N, res & 0x80);

    return clocks;
}

/*
 * INX - Increment X Register
 * Size: 1
 * Cycles: 2
 * Flags: Z, N
 */
static int inx()
{
    assert(state.op == 0xE8);
    mode_imp();
    state.x++;
    //set flags
    set_flag(PSR_Z, state.x == 0);
    set_flag(PSR_N, state.x & 0x80);
    return 2;
}

/*
 * INY - Increment Y Register
 * Size: 1
 * Cycles: 2
 * Flags: Z, N
 */
static int iny()
{
    assert(state.op == 0xC8);
    mode_imp();
    state.y++;
    //set flags
    set_flag(PSR_Z, state.y == 0);
    set_flag(PSR_N, state.y & 0x80);
    return 2;
}

/*
 * JMP - Jump
 * Size: 3
 * Cycles: 3-5
 * Flags: None
 */
static int jmp()
{
    int clocks = 0;
    u16 target;
    u8 dummy;
    switch (state.op) {
    case 0x4C: // ABS -- 3 bytes, 3 cycles
        mode_abs(&dummy, &target);
        clocks = 3;
        break;
    case 0x6C: // IND -- 3 bytes, 5 cycles
        mode_ind(&target);
        clocks = 5;
        break;
    default:
        ERROR("Unknown opcode (%02X)\n", state.op);
        EXIT(1);
    }

    // jump to target
    state.pc = target;

    return clocks;
}

/*
 * JSR - Jump to Subroutine
 * Size: 3
 * Cycles: 6
 * Flags: None
 */
static int jsr()
{
    assert(state.op == 0x20);
    u16 target;
    u8 dummy;
    mode_abs(&dummy, &target);
    // push pc to stack
    cpu_write(state.pc >> 8, SP);
    state.sp--;
    cpu_write(state.pc & 0xFF, SP);
    state.sp--;
    // set subroutine as cur pc
    state.pc = target;
    return 6;
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

