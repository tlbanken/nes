/*
 * _cpu.h
 *
 * Travis Banken
 * 2020
 *
 * Private header file for cpu.c
 */

// I don't want to accidentily include this in any other file other than cpu.c
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
static int mode_imp();
static int mode_rel(u16 *fetch);
static int mode_indx(u8 *fetch, u16 *from);
static int mode_indy(u8 *fetch, u16 *from);
static int mode_ind(u16 *fetch);

// intruction handlers
static int undef();
static int adc();
static int and();
static int asl();
static int bcc();
static int bcs();
static int beq();
static int bit();
static int bmi();
static int bne();
static int bpl();
static int brk();
static int bvc();
static int bvs();
static int clc();
static int cld();
static int cli();
static int clv();
static int cmp();
static int cpx();
static int cpy();
static int dec();
static int dex();
static int dey();
static int eor();
static int inc();
static int inx();
static int iny();
static int jmp();
static int jsr();
static int lda();
static int ldx();
static int ldy();
static int lsr();
static int nop();
static int ora();
static int pha();
static int php();
static int pla();
static int plp();
static int rol();
static int ror();
static int rti();
static int rts();
static int sbc();
static int sec();
static int sed();
static int sei();
static int sta();
static int stx();
static int sty();
static int tax();
static int tay();
static int tsx();
static int txa();
static int txs();
static int tya();


#endif