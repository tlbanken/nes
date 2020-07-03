/*
 * periphs.h
 *
 * Travis Banken
 * 2020
 *
 * Header for the NES Peripherals
 */

#ifndef _PERIPHS_H
#define _PERIPHS_H

#include <utils.h>

typedef struct nes_color {
	u8 red;
	u8 green;
	u8 blue;
} nes_color_t;

void periphs_init();
void periphs_free();
void periphs_refresh();
void periphs_poll();
void set_px(int x, int y, nes_color_t color);
void clear_screen();


#endif