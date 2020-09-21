/*
 * mem.c
 *
 * Travis Banken
 * 2020
 *
 * CPU and PPU memory management. Abstracts all memory for both the cpu and ppu
 * address space.
 */

#include <stdlib.h>

#include <utils.h>
#include <cart.h>
#include <ppu.h>
#include <vac.h>
#include <apu.h>

#define CHECK_INIT if(!is_init){ERROR("Not Initialized!\n"); EXIT(1);}

static bool is_init = false;
void Mem_Init()
{
    is_init = true;
}

// **********************************************************************
// *** CPU ADDRESS SPACE ***
//      CPU Memory Map
// -----------------------
// |   0x0000 - 0x1FFF   |
// |   IRAM (Mirrored)   |
// |       (2 KB)        |
// -----------------------
// |   0x2000 - 0x3FFF   |
// | PPU Regs (Mirrored) |
// |       (8 B)         |
// -----------------------
// |   0x4000 - 0x4017   |
// |     APU/IO Regs     |
// |       (24 B)        |
// -----------------------
// |   0x4018 - 0x401F   |
// | TEST APU/IO (unused)|
// |       (8 B)         |
// -----------------------
// |   0x4020 - 0xFFFF   |
// |   Cartridge Space   |
// |     (49.120 KB)     |
// -----------------------
// **********************************************************************
static u8 iram[2*1024] = {0};
static u8 controller[2] = {0};

u8 Mem_CpuRead(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    // cartridge access (most likely so put this first)
    if (addr >= 0x4020) {
        return Cart_CpuRead(addr);
    }

    // internal ram access
    if (addr <= 0x1FFF) {
        return iram[addr & 0x7FF];
    }

    // ppu register access
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        // convert to 0-7 addr space and read
        return Ppu_RegRead(addr & 0x7);
    }

    // apu/io reads
    if (addr >= 0x4000 && addr <= 0x4017) {
        // TODO read the correct apu/io reg
        u16 res;
        switch (addr) {
        case 0x4016: // Controller 1
            // return next in report
            res = (controller[0] & 0x80) > 0;
            controller[0] <<= 1;
            return res; // upper bits same as addr
        case 0x4017: // Controller 2
            // NOTE: For now, ignore controller 2
            return 0x0;
            // return next in report
            res = (controller[1] & 0x80) > 0;
            controller[1] <<= 1;
            return res; // upper bits same as addr
        default:
            // let the apu handle the address
            return Apu_Read(addr);
        }
        return 0;
    }

    // disabled apu/io reads
    if (addr >= 0x4018 && addr <= 0x401F) {
        // TODO ???
        WARNING("APU/IO test regs not available ($%04X)\n", addr);
        // EXIT(1);
        return 0;
    }

    // Should not get here
    ERROR("Unknown cpu read request addr ($%04X)\n", addr);
    EXIT(1);
    return 0;
}

void Mem_CpuWrite(u8 data, u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    // cartridge access (most likely so put this first)
    if (addr >= 0x4020) {
        Cart_CpuWrite(data, addr);
        return;
    }
    
    // internal ram access
    if (addr <= 0x1FFF) {
        iram[addr & 0x7FF] = data;
        return;
    }
    
    // ppu register access
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        // convert to 0-7 addr space and write
        Ppu_RegWrite(data, addr & 0x7);
        return;
    }
    
    // apu/io access
    if (addr >= 0x4000 && addr <= 0x4017) {
        // TODO read the correct apu/io reg
        switch (addr) {
        case 0x4014:
            Ppu_Oamdma(data);
            break;
        case 0x4016: // Controller 1
            if (data & 0x1) {
                controller[0] = Vac_Poll() & 0xFF;
            }
            break;
        case 0x4017: // Controller 2
            if (data & 0x1) {
                controller[1] = Vac_Poll() & 0xFF;
            }
            break;
        default:
            // let the apu handle the rest of the addresses
            Apu_Write(data, addr);
            break;
        }
        // EXIT(1);
        return;
    }
    
    // disabled apu/io access (not used)
    if (addr >= 0x4018 && addr <= 0x401F) {
        WARNING("APU/IO test regs not available ($%04X)\n", addr);
        return;
    }


    // Should not get here
    ERROR("Unknown cpu write request addr (%02X -> $%04X)\n", data, addr);
    EXIT(1);
}

// **********************************************************************
// *** PPU ADDRESS SPACE ***
//      PPU Memory Map
// -----------------------
// |   0x0000 - 0x1FFF   |
// |   Pattern Tables    |
// |       (8 KB)        |
// -----------------------
// |   0x2000 - 0x2FFF   |
// |      Nametables     |
// |       (4 KB)        |
// -----------------------
// |   0x3000 - 0x3EFF   |
// |  Nametable Mirrors  |
// |       (3,839 B)     |
// -----------------------
// |   0x3F00 - 0x3FFF   |
// |   Palette Control   |
// |       (256 B)       |
// -----------------------
// **********************************************************************
static u8 vram[(4*1024)] = {0};
static u8 palmem[256] = {0};


static u16 mirror(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
    assert(addr >= 0x2000);
#endif

    enum mirror_mode mirror_mode = Cart_GetMirrorMode();
    switch (mirror_mode) {
    case MIR_HORZ:
        // $2000 and $2400 are mirrored
        // $2800 and $2C00 are mirrored
        addr &= ~0x0400;
        break;
    case MIR_VERT:
        // $2000 and $2800 are mirrored
        // $2400 and $2C00 are mirrored
        addr &= ~0x0800;
        break;
    case MIR_4SCRN:
        // no change to addr (no mirroring)
        break;
    case MIR_1LOWER:
        WARNING("Using experimental One Screen Lower Mirroring!\n");
        EXIT(1);
        // everything is mirrored to $2000
        addr &= ~0x0C00;
        break;
    case MIR_1UPPER:
        WARNING("Using experimental One Screen Upper Mirroring!\n");
        EXIT(1);
        // everything is mirrored to $2400
        addr &= ~0x0C00;
        break;
    // case MIR_1LOWER:
    //     ERROR("No support for (1 lower) mirror mode!\n");
    //     EXIT(1);
    //     break;
    case MIR_DEFAULT:
        ERROR("Invalid mirror mode (default)\n");
        EXIT(1);
        break;
    }
    return addr - 0x2000;
}

u8 Mem_PpuRead(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    // Pattern table access
    if (addr <= 0x1FFF) {
        return Cart_PpuRead(addr);
    }

    // Nametable access
    if (addr >= 0x2000 && addr <= 0x2FFF) {
        addr = mirror(addr);
        assert(addr < sizeof(vram));
        return vram[addr];
    }

    // Nametable mirror access
    if (addr >= 0x3000 && addr <= 0x3EFF) {
        addr = mirror(addr - 0x1000);
        assert(addr < sizeof(vram));
        return vram[addr];
    }

    // pallete access
    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // adjust for mirrors
        addr = (addr - 0x3F00) & 0x1F;
        // one byte mirrors
        if (addr == 0x10) {
            addr = 0x00;
        } else if (addr == 0x14) {
            addr = 0x04;
        } else if (addr == 0x18) {
            addr = 0x08;
        } else if (addr == 0x1C) {
            addr = 0x0C;
        }
        assert(addr < sizeof(palmem));
        return palmem[addr];
    }

    // shouldn't be anything mapped past here, but I'll throw a warning instead
    // of crashing
    WARNING("Attempt to read past ppu address $3FFF ($%04X)\n", addr);
    return 0;
}

void Mem_PpuWrite(u8 data, u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    // Pattern table access
    if (addr <= 0x1FFF) {
        Cart_PpuWrite(data, addr);
        return;
    }
    
    // Nametable access
    if (addr >= 0x2000 && addr <= 0x2FFF) {
        addr = mirror(addr);
        assert(addr < sizeof(vram));
        vram[addr] = data;
        return;
    }
    
    // Nametable mirror access
    if (addr >= 0x3000 && addr <= 0x3EFF) {
        addr = mirror(addr - 0x1000);
        assert(addr < sizeof(vram));
        vram[addr] = data;
        return;
    }
    
    // pallete access
    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // adjust for mirrors
        addr = (addr - 0x3F00) & 0x1F;
        // one byte mirrors
        if (addr == 0x10) {
            addr = 0x00;
        } else if (addr == 0x14) {
            addr = 0x04;
        } else if (addr == 0x18) {
            addr = 0x08;
        } else if (addr == 0x1C) {
            addr = 0x0C;
        }
        assert(addr < sizeof(palmem));
        palmem[addr] = data;
        return;
    }

    // shouldn't be anything mapped past here, but I'll throw a warning instead
    // of crashing
    WARNING("Attempt to write past ppu address $3FFF ($%02X -> $%04X)\n", data, addr);
}

// *** DEBUG TOOLS ***
void Mem_Dump()
{
#ifdef DEBUG
    if (!is_init) {
        WARNING("Not Initialized!\n");
    }
#endif
    // dump iram
    FILE *ofile = fopen("iram.dump", "wb");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump IRAM\n");
        return;
    }
    fwrite(iram, 1, sizeof(iram), ofile);
    fclose(ofile);
    ofile = NULL;

    // dump vram
    ofile = fopen("vram.dump", "wb");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump VRAM\n");
        return;
    }
    fwrite(vram, 1, sizeof(vram), ofile);
    fclose(ofile);
    ofile = NULL;

    // dump pallete mem
    ofile = fopen("palmem.dump", "wb");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump PALLETE MEM\n");
        return;
    }
    fwrite(palmem, 1, sizeof(palmem), ofile);
    fclose(ofile);
    ofile = NULL;
}
