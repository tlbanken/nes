/*
 * ppu.h
 *
 * Travis Banken
 * 2020
 *
 * Header for the ppu
 */

#ifndef _PPU_H
#define _PPU_H

#include <utils.h>

void ppu_init();
void ppu_step(int clock_budget);
u8 ppu_reg_read(u16 reg);
void ppu_reg_write(u8 val, u16 reg);
void ppu_oamdma(u8 hi);

#endif