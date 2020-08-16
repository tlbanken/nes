/*
 * map000.c
 *
 * Travis Banken
 * 2020
 *
 * Mapper 0 Boards: NROM, HROM*, RROM, RTROM, SROM, STROM
 */

#include <utils.h>
#include <cart.h>

static size_t prgrom_banks;
static size_t chrrom_banks;

void Map000_Init(u8 _prgrom_banks, u8 _chrrom_banks)
{
    prgrom_banks = _prgrom_banks;
    chrrom_banks = _chrrom_banks;
}

bool Map000_CpuRead(u32 *addr)
{
    // TODO figure out what this is?
    if (*addr < 0x6000) {
        WARNING("Trying to access mystery address ($%04X)\n", *addr);
        // no maps
        return true;
    }

    if (prgrom_banks == 1) {
        *addr &= ~0x4000;
    }
    return true;

}

bool Map000_CpuWrite(u8 data, u32 *addr)
{
    (void) data;
    // PRG-ROM
    if (*addr >= 0x8000) {
        // no writes
        return false;
    }

    // TODO figure out what this is?
    if (*addr < 0x6000) {
        WARNING("Trying to access unsupported address ($%04X)\n", *addr);
        // no maps
        return true;
    }

    // else no maps
    return true;
}

bool Map000_PpuRead(u32 *addr)
{
    (void) addr;
    return true;
}

bool Map000_PpuWrite(u8 data, u32 *addr)
{
    (void) data;
    (void) addr;
    // only writes if pattern mem is ram
    return chrrom_banks == 0;
}

enum mirror_mode Map000_GetMirrorMode()
{
    return MIR_DEFAULT;
}
