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

static size_t prgrom_size;
static size_t chrrom_size;

void Map000_Init(u8 prgrom_banks, u8 chrrom_banks)
{
    prgrom_size = prgrom_banks * PRGROM_BANK_SIZE;
    chrrom_size = chrrom_banks * CHRROM_BANK_SIZE;
}

bool Map000_CpuRead(u16 *addr)
{
    // PRG-RAM
    if (*addr >= 0x6000 && *addr <= 0x7FFF) {
        // no maps
        return true;
    }

    // PRG-ROM
    if (*addr >= 0x8000) {
        *addr &= ~0x4000;
        return true;
    }

    // TODO figure out what this is?
    if (*addr < 0x6000) {
        WARNING("Trying to access unsupported address ($%04X)\n", *addr);
        // no maps
        return true;
    }

    // should not get here
    assert(0);
    return false;
}

bool Map000_CpuWrite(u16 *addr)
{
    // PRG-RAM
    if (*addr >= 0x6000 && *addr <= 0x7FFF) {
        // no maps
        return true;
    }

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

    // should not get here
    assert(0);
    return false;
}

bool Map000_PpuRead(u16 *addr)
{
    (void) addr;
    return true;
}

bool Map000_PpuWrite(u16 *addr)
{
    (void) addr;
    // only writes if pattern mem is ram
    return chrrom_size == 0;
}
