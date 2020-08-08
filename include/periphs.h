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

enum nes_keycode {
	KEY_A          = (1 << 0),
	KEY_B          = (1 << 1),
	KEY_SELECT     = (1 << 2),
	KEY_START	   = (1 << 3),
	KEY_UP 		   = (1 << 4),
	KEY_DOWN       = (1 << 5),
	KEY_LEFT       = (1 << 6),
	KEY_RIGHT      = (1 << 7),
	// DEBUG
	KEY_PAUSE      = (1 << 8),
	KEY_STEP       = (1 << 9),
	KEY_CONTINUE   = (1 << 10),
	KEY_FRAME_MODE = (1 << 11),
	KEY_PAL_CHANGE = (1 << 12),
};

void periphs_init(const char *title, bool debug_display);
void periphs_free();
void periphs_refresh();
u16 periphs_poll();
void set_px(int x, int y, nes_color_t color);
void set_px_pt(int table_side, u16 x, u16 y, nes_color_t color);
void clear_screen();
bool periphs_one_sec_passed();


#endif