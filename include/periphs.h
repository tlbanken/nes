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
	KEY_NONE,
	KEY_UP,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_A,
	KEY_B,
	KEY_START,
	KEY_SELECT,
	// DEBUG
	KEY_PAUSE,
	KEY_STEP,
	KEY_CONTINUE,
	KEY_FRAME_STEP,
};

void periphs_init();
void periphs_free();
void periphs_refresh();
enum nes_keycode periphs_poll();
void set_px(int x, int y, nes_color_t color);
void clear_screen();


#endif