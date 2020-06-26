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

typedef enum mirror_mode {
	MIR_HORZ  = 0,
	MIR_VERT  = 1,
	MIR_4SCRN = 2
} mirror_mode_t;

void cart_load(const char *path);
u16 cart_cpu_map(u16 addr);
u16 cart_ppu_map(u16 addr);
mirror_mode_t cart_get_mirror_mode();
void cart_dump();

#endif