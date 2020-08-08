/*
 * ppu.c
 *
 * Travis Banken
 * 2020
 *
 * Picture Processing Unit for the NES.
 */

#include <utils.h>
#include <mem.h>
#include <vac.h>
#include <cpu.h>

#define LOG(fmt, ...) Neslog_Log(LID_PPU, fmt, ##__VA_ARGS__);
static bool is_init = false;
#define CHECK_INIT if(!is_init){ERROR("Not Initialized!\n"); EXIT(1);}

void Ppu_Init()
{

}

bool Ppu_Step(int clock_budget)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
}

u8 Ppu_RegRead(u16 reg)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
}

void Ppu_RegWrite(u8 val, u16 reg)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
}

void Ppu_Oamdma(u8 hi)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
}

void Ppu_Dump()
{

}
