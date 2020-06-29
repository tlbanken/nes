/*
 * ppu.c
 *
 * Travis Banken
 * 2020
 *
 * Picture Processing Unit for the NES.
 */

#include <utils.h>

u8 ppu_reg_read(u16 reg)
{
	(void) reg;
	WARNING("PPU Registers not implemented\n");
	return 0;
}

void ppu_reg_write(u8 val, u16 reg)
{
	(void) reg;
	(void) val;
	WARNING("PPU Registers not implemented\n");
	return;
}