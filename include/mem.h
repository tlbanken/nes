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

void mem_dump();
void ppu_write(u8 data, u16 addr);
u8 ppu_read(u16 addr);
void cpu_write(u8 data, u16 addr);
u8 cpu_read(u16 addr);

#endif