/*
 * mem.c
 *
 * Travis Banken
 * 2020
 *
 * CPU and PPU memory management. Handles all memory for both the cpu and ppu
 * address space.
 */

#include <stdlib.h>

#include <utils.h>
#include <cart.h>
#include <ppu.h>
#include <vac.h>

#define CHECK_INIT if(!is_init){ERROR("Not Initialized!\n"); EXIT(1);}

static bool is_init = false;
void Mem_Init()
{
    is_init = true;
}

// *** CPU ADDRESS SPACE ***

// Memory map for the cpu address space
enum cpu_memmap {
    // CPU internal ram (mirrored)
    MC_IRAM_START = 0x0000,
    MC_IRAM_END   = 0x1FFF,
    MC_IRAM_SIZE  = (2*1024),

    // PPU registers (mirrored)
    MC_PPU_START = 0x2000,
    MC_PPU_END   = 0x3FFF,
    MC_PPU_SIZE  = 8,

    // APU and IO regs
    MC_APU_IO_START = 0x4000,
    MC_APU_IO_END   = 0x4017,
    MC_APU_IO_SIZE  = 24,

    // Disabled apu and io regs (may be used in test mode?)
    MC_APU_IO_TEST_START = 0x4018,
    MC_APU_IO_TEST_END   = 0x401F,
    MC_APU_IO_TEST_SIZE  = 8,

    // Cartridge Space
    MC_CART_START = 0x4020,
    MC_CART_END   = 0xFFFF,
    MC_CART_SIZE  = (MC_CART_END - MC_CART_START + 1)
};
static u8 iram[MC_IRAM_SIZE] = {0};
static u8 cartmem[MC_CART_SIZE] = {0};
static u8 ctrl1_buf;
static u8 ctrl1_reads;
static u8 ctrl2_buf;
static u8 ctrl2_reads;

u8 Mem_CpuRead(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    // cartridge access (most likely so put this first)
    if (addr >= 0x4020) {
        addr = Cart_CpuMap(addr);
        addr -= 0x4020;
        return cartmem[addr];
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
            // official nes controller returns 1 when report is over
            if (ctrl2_reads == 8) {
                return 0x41;
            }
            // return next in report
            res = (ctrl1_buf >> ctrl1_reads) & 0x1;
            ctrl1_reads++;
            return res | 0x40; // upper bits same as addr
        case 0x4017: // Controller 2
            // NOTE: For now, ignore controller 2
            return 0x41;
            // official nes controller returns 1 when report is over
            if (ctrl2_reads == 8) {
                return 0x41;
            }
            // return next in report
            res = (ctrl2_buf >> ctrl2_reads) & 0x1;
            ctrl2_reads++;
            return res | 0x40; // upper bits same as addr
        default:
            WARNING("APU/IO reg not available ($%04X)\n", addr);
            break;
        }
        // EXIT(1);
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
        addr = Cart_CpuMap(addr);
        addr -= 0x4020;
        cartmem[addr] = data;
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
                ctrl1_buf = Vac_Poll() & 0xFF;
                ctrl1_reads = 0;
            }
            break;
        case 0x4017: // Controller 2
            if (data & 0x1) {
                ctrl2_buf = Vac_Poll() & 0xFF;
                ctrl2_reads = 0;
            }
            break;
        default:
            WARNING("APU/IO reg not available ($%04X)\n", addr);
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

// *** PPU ADDRESS SPACE ***
// Memory map for the ppu address space
enum ppu_memmap {
    // Pattern table found on CHR-ROM from cartridge
    MP_PT_START = 0x0000,
    MP_PT_END   = 0x1FFF,
    MP_PT_SIZE  = (8*1024),

    // Nametables stored on VRAM
    MP_NT_START = 0x2000,
    MP_NT_END   = 0x2FFF,
    MP_NT_SIZE  = (4*1024),

    // Usually a mirror of the nametables up to $2EFF
    MP_NT_MIR_START = 0x3000,
    MP_NT_MIR_END   = 0x3EFF,
    MP_NT_MIR_SIZE  = (MP_NT_MIR_END - MP_NT_MIR_START + 1),

    // Pallete Control
    MP_PAL_START = 0x3F00,
    MP_PAL_END   = 0x3FFF,
    MP_PAL_SIZE  = 256
};
static u8 chrrom[(8*1024)] = {0};
static u8 vram[(4*1024)] = {0};
static u8 palmem[256] = {0};

// Name tables
enum NT_MAP {
    // tile info
    NT_NUM_TILES = 32,

    // table addr
    NT_TOPL = 0x2000,
    NT_TOPR = 0x2400,
    NT_BOTL = 0x2800,
    NT_BOTR = 0x2C00,
    NT_SIZE = 1024 // 1KB
};

// Attribute tables
enum AT_MAP {
    AT_TOPL = 0x23C0,
    AT_TOPR = 0x27C0,
    AT_BOTL = 0x2BC0,
    AT_BOTR = 0x2FC0,
    AT_SIZE = 64
};


static u16 mirror(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
    assert(addr >= 0x2000);
#endif

    enum mirror_mode mirror_mode = Cart_GetMirrorMode();
    switch (mirror_mode) {
    case MIR_HORZ:
        if (addr >= NT_TOPL && addr < NT_TOPR) {
            addr = addr; // no mirror
        } else if (addr < NT_BOTL) {
            addr -= NT_SIZE;
        } else if (addr < NT_BOTR) {
            addr -= NT_SIZE;
        } else {
            addr -= (NT_SIZE * 2);
        }
        break;
    case MIR_VERT:
        if (addr >= NT_TOPL && addr < NT_BOTL) {
            addr = addr; // no mirror needed
        } else {
            addr -= (NT_SIZE * 2);
        }
        break;
    case MIR_4SCRN:
        ERROR("No support for 4 screen mirror mode!\n");
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
        addr = Cart_PpuMap(addr);
        return chrrom[addr];
    }

    // Nametable access
    if (addr >= 0x2000 && addr <= 0x2FFF) {
        addr = mirror(addr);
        return vram[addr];
    }

    // Nametable mirror access
    if (addr >= 0x3000 && addr <= 0x3EFF) {
        addr = mirror(addr - 0x1000);
        return vram[addr];
    }

    // pallete access
    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // adjust for mirrors
        addr = (addr - 0x3F00) & 0xFF;
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
        addr = Cart_PpuMap(addr);
        chrrom[addr] = data;
        return;
    }

    // Nametable access
    if (addr >= 0x2000 && addr <= 0x2FFF) {
        addr = mirror(addr);
        vram[addr] = data;
        return;
    }

    // Nametable mirror access
    if (addr >= 0x3000 && addr <= 0x3EFF) {
        addr = mirror(addr - 0x1000);
        vram[addr] = data;
        return;
    }

    // pallete access
    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // adjust for mirrors
        addr = (addr - 0x3F00) & 0xFF;
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
    fwrite(iram, 1, MC_IRAM_SIZE, ofile);
    fclose(ofile);
    ofile = NULL;

    // dump pgr rom
    ofile = fopen("cartmem.dump", "wb");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump PRG-ROM\n");
        return;
    }
    fwrite(cartmem, 1, MC_CART_SIZE, ofile);
    fclose(ofile);
    ofile = NULL;


    // dump chrrom
    ofile = fopen("chr-rom.dump", "wb");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump CHR-ROM\n");
        return;
    }
    fwrite(chrrom, 1, MP_PT_SIZE, ofile);
    fclose(ofile);
    ofile = NULL;

    // dump vram
    ofile = fopen("vram.dump", "wb");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump VRAM\n");
        return;
    }
    fwrite(vram, 1, MP_NT_SIZE, ofile);
    fclose(ofile);
    ofile = NULL;

    // dump pallete mem
    ofile = fopen("palmem.dump", "wb");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump PALLETE MEM\n");
        return;
    }
    fwrite(palmem, 1, MP_PAL_SIZE, ofile);
    fclose(ofile);
    ofile = NULL;
}
