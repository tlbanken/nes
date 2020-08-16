/*
 * map001.c
 *
 * Travis Banken
 * 2020
 *
 * Mapper 1 Boards: SKROM, SLROM, SNROM
 */

#include <utils.h>
#include <cart.h>

// *** Control Reg Bitfield ***
// BITS
// 0-1: Mirroring (0: one-screen, lower bank; 
//                 1: one-screen, upper bank;
//                 2: vertical; 3: horizontal)
//
// 2-3: PRG-ROM   (0, 1: switch 32 KB at $8000, ignoring low bit of bank num;
//     bank mode   2: fix first bank at $8000 and switch 16 KB at $C000;
//                 3: fix last bank at $C000 and switch 16 KB bank at $8000)
//
// 4:   CHR-ROM   (0: switch 8 KB at a time; 1: switch two separate 4 KB banks)
//     bank mode

// Registers
static u8 loadreg;
static u8 ctrlreg;
static u8 chrbank0;
static u8 chrbank1;
static u8 prgbank;
static u8 shifts;

// Number of banks
static u8 prgrom_banks;
static u8 chrrom_banks;

// current mirror mode
static enum mirror_mode mirmode;

void Map001_Init(u8 _prgrom_banks, u8 _chrrom_banks)
{
    // init regs
    loadreg  = 0x00;
    ctrlreg  = 0x1C;
    chrbank0 = 0x00;
    chrbank1 = 0x00;
    prgbank  = 0x00;
    shifts = 0;

    // init banks
    prgrom_banks = _prgrom_banks;
    chrrom_banks = _chrrom_banks;

    // init mirror mode 
    mirmode = MIR_DEFAULT;
}

bool Map001_CpuRead(u32 *addr)
{
    if (*addr >= 0x8000) {
        u8 prgrom_bank_mode = (ctrlreg >> 2) & 0x3;
        if (prgrom_bank_mode == 0 || prgrom_bank_mode == 1) {
            // 32 KB switching at 0x8000
            *addr = *addr + ((prgbank >> 1) * 0x8000);
        } else if (prgrom_bank_mode == 2) {
            // fix first bank at $8000
            *addr = *addr < 0xC000
                ? *addr
                : ((*addr - 0x4000) + (prgbank * 0x4000));
        } else {
            // fix last bank at $C000
            *addr = *addr >= 0xC000
                ? (*addr - 0x4000) + ((prgrom_banks - 1) * 0x4000)
                : (*addr + (prgbank * 0x4000));
        }
    }

    return true;
}

bool Map001_CpuWrite(u8 data, u32 *addr)
{
    if (*addr >= 0x8000) {
        if (data & 0x80) {
            // reset
            loadreg = 0x00;
            shifts = 0;
        } else {
            loadreg >>= 1;
            shifts++;
            loadreg |= (data & 0x01) << 4;
            if (shifts == 5) {
                // update the internal register
                switch ((*addr >> 13) & 0xF) {
                case 0b100: // CONTROL
                    ctrlreg = loadreg;
                    // set cur mirror mode
                    switch (ctrlreg & 0x3) {
                    case 0:
                        mirmode = MIR_1LOWER;
                        break;
                    case 1:
                        mirmode = MIR_1UPPER;
                        break;
                    case 2:
                        mirmode = MIR_VERT;
                        break;
                    case 3:
                        mirmode = MIR_HORZ;
                        break;
                    }
                    break;
                case 0b101: // CHR BANK 0
                    chrbank0 = loadreg;
                    break;
                case 0b110: // CHR BANK 1
                    chrbank1 = loadreg;
                    break;
                case 0b111: // PRG BANK
                    prgbank = loadreg;
                    break;
                default:
                    ERROR("This shouldn't print! Check your bitwise math!\n");
                    EXIT(1);
                }

                // reset
                loadreg = 0x00;
                shifts = 0;
            }
        }
        return false;
    } else {
        return true;
    }
}

static u32 chrmap_helper(u32 addr)
{
    u8 chrbank_mode = (ctrlreg >> 4) & 0x1;
    if (chrbank_mode == 1) {
        // 4KB mode
        if (addr < 0x1000) {
            addr = addr + (chrbank0 * 0x1000);
        } else {
            addr = (addr - 0x1000) + (chrbank1 * 0x1000);
        }
    } else {
        // 8KB mode
        addr = addr + ((chrbank0 >> 1) * 0x2000);
    }
    return addr;
}

bool Map001_PpuRead(u32 *addr)
{
    *addr = chrmap_helper(*addr);
    return true;
}

bool Map001_PpuWrite(u8 data, u32 *addr)
{
    (void) data;
    if (chrrom_banks != 0) {
        // using ROM -- no write access!
        return false;
    }

    *addr = chrmap_helper(*addr);
    return true;
}

enum mirror_mode Map001_GetMirrorMode()
{
    return mirmode;
}



