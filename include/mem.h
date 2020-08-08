/*
 * mem.h
 *
 * Travis Banken
 * 2020
 *
 * Header for the nes memory management
 */

#ifndef _MEM_H
#define _MEM_H

#include <cart.h>

void Mem_Init();
void Mem_Dump();
void Mem_PpuWrite(u8 data, u16 addr);
u8 Mem_PpuRead(u16 addr);
void Mem_CpuWrite(u8 data, u16 addr);
u8 Mem_CpuRead(u16 addr);

#endif