/*
 * _cpu.h
 *
 * Travis Banken
 * 2020
 *
 * Private header file for cpu.c
 */

// I don't want to accidentily include this in any other file other than cpu.h
#ifdef __CPU_H
#error "Only one include of _cpu.h allowed!\n"
#else
#define __CPU_H

// address modes
static int mode_acc(u8 *fetch);
static int mode_imm(u8 *fetch);
static int mode_abs(u8 *fetch, u16 *from);
static int mode_zp(u8 *fetch, u16 *from);
static int mode_zpx(u8 *fetch, u16 *from);
static int mode_zpy(u8 *fetch, u16 *from);
static int mode_absx(u8 *fetch, u16 *from);
static int mode_absy(u8 *fetch, u16 *from);
static int mode_imp(u8 *fetch, u16 *from);
static int mode_rel(u8 *fetch, u16 *from);
static int mode_indx(u8 *fetch, u16 *from);
static int mode_indy(u8 *fetch, u16 *from);
static int mode_ind(u8 *fetch, u16 *from);

// intruction handlers
static int undef(instr_t instr);
static int adc(instr_t instr);
static int and(instr_t instr);
static int asl(instr_t instr);
static int bcc(instr_t instr);
static int bcs(instr_t instr);
static int beq(instr_t instr);
static int bit(instr_t instr);
static int bmi(instr_t instr);
static int bne(instr_t instr);
static int bpl(instr_t instr);
static int brk(instr_t instr);
static int bvc(instr_t instr);
static int bvs(instr_t instr);
static int clc(instr_t instr);
static int cld(instr_t instr);
static int cli(instr_t instr);
static int clv(instr_t instr);
static int cmp(instr_t instr);
static int cpx(instr_t instr);
static int cpy(instr_t instr);
static int dec(instr_t instr);
static int dex(instr_t instr);
static int dey(instr_t instr);
static int eor(instr_t instr);
static int inc(instr_t instr);
static int inx(instr_t instr);
static int iny(instr_t instr);
static int jmp(instr_t instr);
static int jsr(instr_t instr);
static int lda(instr_t instr);
static int ldx(instr_t instr);
static int ldy(instr_t instr);
static int lsr(instr_t instr);
static int nop(instr_t instr);
static int ora(instr_t instr);
static int pha(instr_t instr);
static int php(instr_t instr);
static int pla(instr_t instr);
static int plp(instr_t instr);
static int rol(instr_t instr);
static int ror(instr_t instr);
static int rti(instr_t instr);
static int rts(instr_t instr);
static int sbc(instr_t instr);
static int sec(instr_t instr);
static int sed(instr_t instr);
static int sei(instr_t instr);
static int sta(instr_t instr);
static int stx(instr_t instr);
static int sty(instr_t instr);
static int tax(instr_t instr);
static int tay(instr_t instr);
static int tsx(instr_t instr);
static int txa(instr_t instr);
static int txs(instr_t instr);
static int tya(instr_t instr);


#endif