/*
 * cart.h
 *
 * Travis Banken
 * 2020
 *
 * Header for the cartridge module.
 */

#ifndef _CART_H
#define _CART_H

#include <utils.h>

enum mirror_mode {
	MIR_HORZ,
	MIR_VERT,
	MIR_4SCRN,
	MIR_1LOWER,
	MIR_1UPPER,
	MIR_DEFAULT,
};

#define PRGROM_BANK_SIZE (16*1024)
#define CHRROM_BANK_SIZE (8*1024)

void Cart_Init();
void Cart_Load(const char *path);
u8 Cart_CpuRead(u16 addr);
void Cart_CpuWrite(u8 data, u16 addr);
u8 Cart_PpuRead(u16 addr);
void Cart_PpuWrite(u8 data, u16 addr);
enum mirror_mode Cart_GetMirrorMode();
void Cart_Dump();

#endif