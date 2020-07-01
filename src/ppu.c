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

// Object Attrubute Memory
static u8 oam[256] = {0};

typedef struct oam_entry {
    u8 y;    // sprite y-coordinate
    u8 tile; // tile number
    u8 attr; // sprite attribute
    u8 x;    // sprite x-coordinate
} oam_entry_t;

// Registers
// $2000
typedef union reg_ppuctrl {
    struct ppuctrl_field {
        u8 x_nt: 1;
        u8 y_nt: 1;
        u8 vram_incr: 1;    // 0: 1, 1: 32
        u8 sprite_side: 1;  // 0: $0000, 1: $1000
        u8 bg_side: 1;      // 0: $0000, 1: $1000
        u8 sprite_size: 1;  // 0: 8x8, 1: 16x16
        u8 master_slave: 1; // 0: read from EXT pins, 1: output color on pins
        u8 nmi_gen: 1;      // 0: No NMI on vblank start
    } field;
    u8 raw;
} reg_ppuctrl_t;
static reg_ppuctrl_t ppuctrl;

// $2001
typedef union reg_ppumask {
    struct ppumask_field {
        u8 greyscale: 1;        // 0: normal color, 1: greyscale color
        u8 render_lbg: 1;       // 0: hide, 1: show
        u8 render_lsprites: 1;  // 0: hide, 1: show
        u8 render_bg: 1;        // 0: hide, 1: show
        u8 render_sprites: 1;   // 0: hide, 1: show
        u8 emph_red: 1;
        u8 emph_green: 1;
        u8 emph_blue: 1;
    } field;
    u8 raw;
} reg_ppumask_t;
static reg_ppumask_t ppumask;

// $2002
typedef union reg_ppustatus {
    struct ppustatus_field {
        u8 last_5lsb: 5;        // last 5 lsb writen to a ppu reg
        u8 sprite_overflow: 1;  // set when more than 8 sprites on scanline (tho bugs are present)
        u8 sprite0_hit: 1;      // set when nonzero sprite0 overlaps with nonzero bg
        u8 vblank: 1;           // set during vertical blanking
    } field;
    u8 raw;
} reg_ppustatus_t;
static reg_ppustatus_t ppustatus;

// $2003
static u8 oamaddr;

// $2006
typedef union loopyreg {
    struct loopyreg_field {
        u16 course_x: 5;
        u16 course_y: 5;
        u16 x_nt: 1;
        u16 y_nt: 1;
        u16 fine_y: 3;
        u16 unused: 1;
    } field;
    u16 raw;
} loopyreg_t;
static loopyreg_t ppuaddr;
static loopyreg_t ppuaddr_tmp;

// other state vars
static u8 fine_x;
static bool al_first_write = true;
static u8 ppudata_buf;

void ppu_init()
{

}

void ppu_step(int clock_budget)
{
    (void) clock_budget;
}

u8 ppu_reg_read(u16 reg)
{
    u8 data = 0;
    switch (reg) {
    case 0: // PPUCTRL
        // no read access
        break;
    case 1: // PPUMASK
        // no read access
        break;
    case 2: // PPUSTATUS
        data = ppustatus.raw;
        ppustatus.field.vblank = 0;
        al_first_write = true;
        break;
    case 3: // OAMADDR
        // no read access
        break;
    case 4: // OAMDATA
        data = oam[oamaddr];
        break;
    case 5: // PPUSCROLL
        // no read access
        break;
    case 6: // PPUADDR
        // no read access
        break;
    case 7: // PPUDATA
        data = ppudata_buf;
        ppudata_buf = ppu_read(ppuaddr.raw);
        if (ppuaddr.raw >= 0x3F00) { // no delay from CHR-ROM
            data = ppudata_buf;
        }
        ppuaddr.raw += (ppuctrl.field.vram_incr ? 32 : 1);
        break;
    default:
        ERROR("Unknown ppu register (%u)\n", reg);
        EXIT(1);
    }
    return data;
}

void ppu_reg_write(u8 val, u16 reg)
{
    switch (reg) {
    case 0: // PPUCTRL
        ppuctrl.raw = val;
        ppuaddr_tmp.field.x_nt = ppuctrl.field.x_nt;
        ppuaddr_tmp.field.y_nt = ppuctrl.field.y_nt;
        break;
    case 1: // PPUMASK
        ppumask.raw = val;
        break;
    case 2: // PPUSTATUS
        // no write access
        break;
    case 3: // OAMADDR
        oamaddr = val;
        break;
    case 4: // OAMDATA
        oam[oamaddr] = val;
        oamaddr++;
        break;
    case 5: // PPUSCROLL
        if (al_first_write) {
            ppuaddr_tmp.field.course_x = (val >> 3);
            fine_x = (val & 0x7);
        } else {
            ppuaddr_tmp.field.course_y = (val >> 3);
            ppuaddr_tmp.field.fine_y = (val & 0x7);
        }
        al_first_write = !al_first_write;
        break;
    case 6: // PPUADDR
        if (al_first_write) {
            ppuaddr_tmp.raw = (((u16)val & 0x3F) << 8) | (ppuaddr_tmp.raw & 0x00FF);
            ppuaddr_tmp.field.fine_y &= 0x3; // turn off bit 14
        } else {
            ppuaddr_tmp.raw = (ppuaddr_tmp.raw & 0xFF00) | val;
            ppuaddr.raw = ppuaddr_tmp.raw; // copy over temp to cur
        }
        al_first_write = !al_first_write;
        break;
    case 7: // PPUDATA
        ppu_write(val, ppuaddr.raw);
        ppuaddr.raw += (ppuctrl.field.vram_incr ? 32 : 1);
        break;
    default:
        ERROR("Unknown ppu register (%u)\n", reg);
        EXIT(1);
    }
    return;
}

// load 256 bytes from $XX00-$XXFF where XX = hi into oam
void ppu_oamdma(u8 hi)
{
    for (u16 lo = 0; lo < 256; lo++) {
        u16 addr = ((u16)hi) << 8;
        addr |= lo;
        u8 val = cpu_read(addr);
        oam[oamaddr] = val;
        oamaddr++;
    }
}
