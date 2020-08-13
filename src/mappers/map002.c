/*
 * map002.c
 *
 * Travis Banken
 * 2020
 *
 * Mapper 002 Boards: UNROM, UOROM
 */

#include <utils.h>

static u8 prgrom_banks;
static u8 chrrom_banks;

// Register
static u8 prgrom_bank_select;

void Map002_Init(u8 _prgrom_banks, u8 _chrrom_banks)
{
    chrrom_banks = _chrrom_banks;
    prgrom_banks = _prgrom_banks;
    prgrom_bank_select = 0x00;
}

bool Map002_CpuRead(u32 *addr)
{
    // first 16 KB in PRGROM
    if (*addr >= 0x8000 && *addr <= 0xBFFF) {
        *addr = *addr + (prgrom_bank_select << 14);
        return true;
    }

    // last 16 KB in PRGROM
    if (*addr >= 0xC000) {
        // select the last bank
        *addr = *addr + ((prgrom_banks - 2) << 14);
        return true;
    }

    return true;
}

bool Map002_CpuWrite(u8 data, u32 *addr)
{
    // PRG-ROM (register access)
    if (*addr >= 0x8000) {
        prgrom_bank_select = data & 0x0F;
        return false;
    }

    return true;
}

bool Map002_PpuRead(u32 *addr)
{
    (void) addr;
    return true;
}

bool Map002_PpuWrite(u8 data, u32 *addr)
{
    assert(*addr < 0x2000);
    (void) addr;
    (void) data;
    return chrrom_banks == 0;
}

