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

typedef struct instr {
    u8 opcode;
} instr_t;

// static function prototypes
#include "_cpu.h"

#define SP (0x0100 | sp)

// interrupt vector locations
#define NMI_VECTOR 0xFFFA
#define RESET_VECTOR 0xFFFC
#define IRQ_VECTOR 0xFFFE

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

typedef int(*op_func)(instr_t);
static op_func opmatrix[NUM_OPS];

typedef struct cpu_state {
    // Registers
    u8 acc;
    u8 x;
    u8 y;
    u8 psr;
    u8 sp;
    u8 pc;

    u32 cycle;
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
    // fetch instruction
    // low 4 bits = LSD
    // high 4 bits = MSD
    u8 opcode = cpu_read(state.pc++);
    int op_index = ((opcode >> 4) & 0xF) * 16 + (opcode & 0xF); 
    instr_t instr;
    instr.opcode = opcode;
    // execute instruction
    int clocks = opmatrix[op_index](instr);
    assert(clocks != 0);
    state.cycle += clocks;
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
    // state.pc = 0xC000; // NOTE: FOR TESTING
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

// *** ADDRESS MODE HANDLERS ***
static int mode_acc(u8 *fetch)
{
    (void) fetch;
    return 0;
}
static int mode_imm(u8 *fetch)
{
    (void) fetch;
    return 0;
}
static int mode_abs(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_zp(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_zpx(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_zpy(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_absx(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_absy(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_imp(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_rel(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_indx(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_indy(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}
static int mode_ind(u8 *fetch, u16 *from)
{
    (void) fetch;
    return 0;
}


// *** INSTRUCTION HANDLERS ***
static int undef(instr_t instr)
{
    (void) instr;
    return 0;
}

static int adc(instr_t instr)
{
    (void) instr;
    return 0;
}

static int and(instr_t instr)
{
    (void) instr;
    return 0;
}

static int asl(instr_t instr)
{
    (void) instr;
    return 0;
}

static int bcc(instr_t instr)
{
    (void) instr;
    return 0;
}

static int bcs(instr_t instr)
{
    (void) instr;
    return 0;
}

static int beq(instr_t instr)
{
    (void) instr;
    return 0;
}

static int bit(instr_t instr)
{
    (void) instr;
    return 0;
}

static int bmi(instr_t instr)
{
    (void) instr;
    return 0;
}

static int bne(instr_t instr)
{
    (void) instr;
    return 0;
}

static int bpl(instr_t instr)
{
    (void) instr;
    return 0;
}

static int brk(instr_t instr)
{
    (void) instr;
    return 0;
}

static int bvc(instr_t instr)
{
    (void) instr;
    return 0;
}

static int bvs(instr_t instr)
{
    (void) instr;
    return 0;
}

static int clc(instr_t instr)
{
    (void) instr;
    return 0;
}

static int cld(instr_t instr)
{
    (void) instr;
    return 0;
}

static int cli(instr_t instr)
{
    (void) instr;
    return 0;
}

static int clv(instr_t instr)
{
    (void) instr;
    return 0;
}

static int cmp(instr_t instr)
{
    (void) instr;
    return 0;
}

static int cpx(instr_t instr)
{
    (void) instr;
    return 0;
}

static int cpy(instr_t instr)
{
    (void) instr;
    return 0;
}

static int dec(instr_t instr)
{
    (void) instr;
    return 0;
}

static int dex(instr_t instr)
{
    (void) instr;
    return 0;
}

static int dey(instr_t instr)
{
    (void) instr;
    return 0;
}

static int eor(instr_t instr)
{
    (void) instr;
    return 0;
}

static int inc(instr_t instr)
{
    (void) instr;
    return 0;
}

static int inx(instr_t instr)
{
    (void) instr;
    return 0;
}

static int iny(instr_t instr)
{
    (void) instr;
    return 0;
}

static int jmp(instr_t instr)
{
    (void) instr;
    return 0;
}

static int jsr(instr_t instr)
{
    (void) instr;
    return 0;
}

static int lda(instr_t instr)
{
    (void) instr;
    return 0;
}

static int ldx(instr_t instr)
{
    (void) instr;
    return 0;
}

static int ldy(instr_t instr)
{
    (void) instr;
    return 0;
}

static int lsr(instr_t instr)
{
    (void) instr;
    return 0;
}

static int nop(instr_t instr)
{
    (void) instr;
    return 0;
}

static int ora(instr_t instr)
{
    (void) instr;
    return 0;
}

static int pha(instr_t instr)
{
    (void) instr;
    return 0;
}

static int php(instr_t instr)
{
    (void) instr;
    return 0;
}

static int pla(instr_t instr)
{
    (void) instr;
    return 0;
}

static int plp(instr_t instr)
{
    (void) instr;
    return 0;
}

static int rol(instr_t instr)
{
    (void) instr;
    return 0;
}

static int ror(instr_t instr)
{
    (void) instr;
    return 0;
}

static int rti(instr_t instr)
{
    (void) instr;
    return 0;
}

static int rts(instr_t instr)
{
    (void) instr;
    return 0;
}

static int sbc(instr_t instr)
{
    (void) instr;
    return 0;
}

static int sec(instr_t instr)
{
    (void) instr;
    return 0;
}

static int sed(instr_t instr)
{
    (void) instr;
    return 0;
}

static int sei(instr_t instr)
{
    (void) instr;
    return 0;
}

static int sta(instr_t instr)
{
    (void) instr;
    return 0;
}

static int stx(instr_t instr)
{
    (void) instr;
    return 0;
}

static int sty(instr_t instr)
{
    (void) instr;
    return 0;
}

static int tax(instr_t instr)
{
    (void) instr;
    return 0;
}

static int tay(instr_t instr)
{
    (void) instr;
    return 0;
}

static int tsx(instr_t instr)
{
    (void) instr;
    return 0;
}

static int txa(instr_t instr)
{
    (void) instr;
    return 0;
}

static int txs(instr_t instr)
{
    (void) instr;
    return 0;
}

static int tya(instr_t instr)
{
    (void) instr;
    return 0;
}

