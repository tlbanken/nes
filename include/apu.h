/*
 *
 */

#ifndef _APU_H
#define _APU_H

void Apu_Init();
void Apu_Reset();
void Apu_Step(int cycle_budget, u32 keystate);
u8 Apu_Read(u16 addr);
void Apu_Write(u8 data, u16 addr);

#endif
