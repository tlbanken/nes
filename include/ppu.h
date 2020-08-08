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

void Ppu_Init();
bool Ppu_Step(int clock_budget);
u8 Ppu_RegRead(u16 reg);
void Ppu_RegWrite(u8 val, u16 reg);
void Ppu_Oamdma(u8 hi);
void Ppu_Dump();
// void draw_pattern_table(u16 table_id, u8 pal_id);

#endif