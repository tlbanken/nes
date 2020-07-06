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

void cart_load(const char *path);
u16 cart_cpu_map(u16 addr);
u16 cart_ppu_map(u16 addr);
enum mirror_mode cart_get_mirror_mode();
void cart_dump();

#endif