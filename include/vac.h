/*
 * periphs.h
 *
 * Travis Banken
 * 2020
 *
 * Header for the wrapper framework for Video, Audio, and Controller
 */

#ifndef _VAC_H
#define _VAC_H

#include <utils.h>

typedef struct nes_color {
    u8 red;
    u8 green;
    u8 blue;
} nes_color_t;

enum nes_keycode {
    KEY_RIGHT      = (1 << 0),
    KEY_LEFT       = (1 << 1),
    KEY_DOWN       = (1 << 2),
    KEY_UP         = (1 << 3),
    KEY_START      = (1 << 4),
    KEY_SELECT     = (1 << 5),
    KEY_B          = (1 << 6),
    KEY_A          = (1 << 7),
    // DEBUG
    KEY_PAUSE      = (1 << 8),
    KEY_STEP       = (1 << 9),
    KEY_CONTINUE   = (1 << 10),
    KEY_FRAME_MODE = (1 << 11),
    KEY_PAL_CHANGE = (1 << 12),
    KEY_RESET      = (1 << 13),
};

void Vac_Init(const char *title, bool debug_display);
void Vac_Free();
void Vac_Refresh();
u16 Vac_Poll();
void Vac_SetPx(int x, int y, nes_color_t color);
void Vac_SetPxPt(int table_side, u16 x, u16 y, nes_color_t color);
void Vac_SetPxNt(int table_side, u16 x, u16 y, nes_color_t color);
void Vac_ClearScreen();
unsigned int Vac_MsPassedFrom(unsigned int from);
bool Vac_OneSecPassed();
void Vac_SetWindowTitle(const char *title);


#endif