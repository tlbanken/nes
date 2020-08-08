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
	MIR_HORZ  = 0,
	MIR_VERT  = 1,
	MIR_4SCRN = 2
};

void Cart_Init();
void Cart_Load(const char *path);
u16 Cart_CpuMap(u16 addr);
u16 Cart_PpuMap(u16 addr);
enum mirror_mode Cart_GetMirrorMode();
void Cart_Dump();

#endif