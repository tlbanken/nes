/*
 * cpu.h
 *
 * Travis Banken
 * 2020
 *
 * Header for the nes cpu implementation
 */

#ifndef _CPU_H
#define _CPU_H

void Cpu_Init();
int Cpu_Step();
void Cpu_Irq();
void Cpu_Nmi();
void Cpu_Reset();

#endif
