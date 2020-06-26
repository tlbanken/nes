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

int cpu_step();
void cpu_reset();
void cpu_init();
void cpu_irq();
void cpu_nmi();
void cpu_reset();

#endif
