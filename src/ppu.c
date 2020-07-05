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
#include <periphs.h>
#include <cpu.h>

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
// https://wiki.nesdev.com/w/index.php/PPU_scrolling
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

// screen state
#define NUM_CYCLES 341
#define NUM_SCANLINES 262
static int cycle;
static int scanline;
static bool oddframe = false;

// shifters
u16 bgshifter_ptrn_lo;
u16 bgshifter_ptrn_hi;
u16 bgshifter_attr_lo;
u16 bgshifter_attr_hi;

// tile buffers
u8 nx_bgtile_id;
u16 nx_bgtile;
u8 nx_bgtile_attr;

// All of the NES Colors
static nes_color_t nes_colors[] = 
{
    /*0x00 -> */{0x59, 0x59, 0x59},
    /*0x01 -> */{0x00, 0x09, 0x89},
    /*0x02 -> */{0x17, 0x00, 0x8a},
    /*0x03 -> */{0x37, 0x00, 0x6e},
    /*0x04 -> */{0x54, 0x00, 0x51},
    /*0x05 -> */{0x54, 0x00, 0x0e},
    /*0x06 -> */{0x54, 0x0a, 0x00},
    /*0x07 -> */{0x3b, 0x17, 0x00},
    /*0x08 -> */{0x22, 0x26, 0x00},
    /*0x09 -> */{0x0a, 0x2a, 0x00},
    /*0x0a -> */{0x00, 0x2b, 0x00},
    /*0x0b -> */{0x00, 0x29, 0x27},
    /*0x0c -> */{0x00, 0x22, 0x59},
    /*0x0d -> */{0x00, 0x00, 0x00},
    /*0x0e -> */{0x00, 0x00, 0x00},
    /*0x0f -> */{0x00, 0x00, 0x00},

    /*0x10 -> */{0xa6, 0xa6, 0xa6},
    /*0x11 -> */{0x00, 0x3b, 0xc5},
    /*0x12 -> */{0x47, 0x25, 0xf6},
    /*0x13 -> */{0x6c, 0x00, 0xe1},
    /*0x14 -> */{0x95, 0x0a, 0xae},
    /*0x15 -> */{0x9e, 0x0e, 0x4d},
    /*0x16 -> */{0x8c, 0x28, 0x00},
    /*0x17 -> */{0x7a, 0x41, 0x00},
    /*0x18 -> */{0x59, 0x50, 0x00},
    /*0x19 -> */{0x23, 0x57, 0x00},
    /*0x1a -> */{0x00, 0x5e, 0x00},
    /*0x1b -> */{0x00, 0x5e, 0x44},
    /*0x1c -> */{0x00, 0x53, 0x87},
    /*0x1d -> */{0x00, 0x00, 0x00},
    /*0x1e -> */{0x00, 0x00, 0x00},
    /*0x1f -> */{0x00, 0x00, 0x00},

    /*0x20 -> */{0xe6, 0xe6, 0xe6},
    /*0x21 -> */{0x4c, 0x88, 0xff},
    /*0x22 -> */{0x70, 0x75, 0xff},
    /*0x23 -> */{0x90, 0x5c, 0xff},
    /*0x24 -> */{0xb4, 0x5a, 0xe1},
    /*0x25 -> */{0xc7, 0x5a, 0x99},
    /*0x26 -> */{0xd4, 0x6d, 0x48},
    /*0x27 -> */{0xc7, 0x83, 0x06},
    /*0x28 -> */{0xae, 0x9c, 0x00},
    /*0x29 -> */{0x6c, 0xa6, 0x00},
    /*0x2a -> */{0x2e, 0xab, 0x2e},
    /*0x2b -> */{0x28, 0xb0, 0x7a},
    /*0x2c -> */{0x1f, 0xaf, 0xcc},
    /*0x2d -> */{0x40, 0x40, 0x40},
    /*0x2e -> */{0x00, 0x00, 0x00},
    /*0x2f -> */{0x00, 0x00, 0x00},

    /*0x30 -> */{0xe6, 0xe6, 0xe6},
    /*0x31 -> */{0xa2, 0xc3, 0xf3},
    /*0x32 -> */{0xad, 0xad, 0xf8},
    /*0x33 -> */{0xb7, 0xa2, 0xf3},
    /*0x34 -> */{0xcc, 0xa8, 0xe1},
    /*0x35 -> */{0xd9, 0xa9, 0xd0},
    /*0x36 -> */{0xd9, 0xae, 0xa3},
    /*0x37 -> */{0xd9, 0xbb, 0x91},
    /*0x38 -> */{0xd9, 0xd0, 0x8d},
    /*0x39 -> */{0xbf, 0xd7, 0x90},
    /*0x3a -> */{0xae, 0xd9, 0xa5},
    /*0x3b -> */{0xa1, 0xd9, 0xbe},
    /*0x3c -> */{0xa1, 0xcf, 0xd9},
    /*0x3d -> */{0xab, 0xab, 0xab},
    /*0x3e -> */{0x00, 0x00, 0x00},
    /*0x3f -> */{0x00, 0x00, 0x00}
};

static inline bool in_render_zone()
{
    return ((cycle >= 1 && cycle <= 257) || (cycle >= 321 && cycle <= 340)) && 
            ((scanline >= 0 || scanline <= 239) || scanline == 261);
}

static void render_px()
{
    if (ppumask.field.render_bg) {
        u16 fine_bit = 0x1 << fine_x;
        u8 px0 = bgshifter_ptrn_lo & fine_bit ? 1 : 0;
        u8 px1 = bgshifter_ptrn_hi & fine_bit ? 1 : 0;
        u8 bg_px = (px1 << 1) | px0;

        u8 pal0 = bgshifter_attr_lo & fine_bit ? 1 : 0;
        u8 pal1 = bgshifter_attr_hi & fine_bit ? 1 : 0;
        u8 bg_pal = (pal1 << 1) | pal0;

        // get color
        u16 addr = 0x3F00;     // Pallete range
        addr += (bg_pal << 2); // 4 byte sized palletes
        addr += bg_px;         // pixel index
        u8 col_id = ppu_read(addr) % 64;
        nes_color_t col = nes_colors[col_id];

        // draw the pixel
        set_px(cycle, scanline, col);
    }
}

// increment course_x as described here:
// https://wiki.nesdev.com/w/index.php/PPU_scrolling
static void scroll_horz()
{
    if (ppumask.field.render_bg || ppumask.field.render_sprites) {
        if (ppuaddr.field.course_x == 31) {
            ppuaddr.field.course_x = 0;
            ppuaddr.field.x_nt = !ppuaddr.field.x_nt;
        } else {
            ppuaddr.field.course_x++;
        }
    }
}

// reset course x to temp course x
static void reset_horz()
{
    if (ppumask.field.render_bg || ppumask.field.render_sprites) {
        ppuaddr.field.course_x = ppuaddr_tmp.field.course_x;
        ppuaddr.field.x_nt = ppuaddr_tmp.field.x_nt;
    }
}

// increment course_y as described here:
// https://wiki.nesdev.com/w/index.php/PPU_scrolling
static void scroll_vert()
{
    // check that rendering is on, then increment y
    if (ppumask.field.render_bg || ppumask.field.render_sprites) {
        // increment fine_y until overflow
        if (ppuaddr.field.fine_y < 7) {
            ppuaddr.field.fine_y += 1;
        } else {
            ppuaddr.field.fine_y = 0;
            if (ppuaddr.field.course_y == 29) {
                ppuaddr.field.course_y = 0;
                ppuaddr.field.y_nt = !ppuaddr.field.y_nt;
            } else if (ppuaddr.field.course_y == 31) {
                ppuaddr.field.course_y = 0;
            } else {
                ppuaddr.field.course_y++;
            }
        }
    }
}

// reset course y to temp course y
static void reset_vert()
{
    if (ppumask.field.render_bg || ppumask.field.render_sprites) {
        ppuaddr.field.course_x = ppuaddr_tmp.field.course_y;
        ppuaddr.field.y_nt = ppuaddr_tmp.field.y_nt;
    }
}

static void shift_bgshifters()
{
    if (ppumask.field.render_bg) {
        bgshifter_ptrn_lo >>= 1;
        bgshifter_ptrn_hi >>= 1;
        bgshifter_attr_lo >>= 1;
        bgshifter_attr_hi >>= 1;
    }
}

static void load_bgshifters()
{
    // Every cycle we use the lsb of the pttrn and attr shifters to render the
    // pixel. Here we load the future byte into the msb of the 16-bit shifters

    // pattern bits
    bgshifter_ptrn_lo = (bgshifter_ptrn_lo & 0x00FF) | ((nx_bgtile & 0xFF) << 8);
    bgshifter_ptrn_hi = (bgshifter_ptrn_hi & 0x00FF) | (nx_bgtile & 0xFF00);

    // attribute bits
    bgshifter_attr_lo = (bgshifter_attr_lo & 0x00FF) | (nx_bgtile_attr & 0x1 ? 0xFF00 : 0x00);
    bgshifter_attr_hi = (bgshifter_attr_hi & 0x00FF) | (nx_bgtile_attr & 0x2 ? 0xFF00 : 0x00);
}

void ppu_init()
{
    // sanity check union-struct hacks
    assert(sizeof(reg_ppuctrl_t) == 1);
    assert(sizeof(reg_ppumask_t) == 1);
    assert(sizeof(reg_ppustatus_t) == 1);
    assert(sizeof(loopyreg_t) == 2);


    // setup initial state
    cycle = 0;
    scanline = 261;
    al_first_write = true;
    ppudata_buf = 0;
    oddframe = false;

    ppuctrl.raw = 0;
    ppumask.raw = 0;
    ppustatus.raw = 0;
    oamaddr = 0;

    ppuaddr.raw = 0;
    ppuaddr_tmp.raw = 0;

    fine_x = 0;

    bgshifter_ptrn_lo = 0;
    bgshifter_ptrn_hi = 0;
    bgshifter_attr_lo = 0;
    bgshifter_attr_hi = 0;
    nx_bgtile_id = 0;
    nx_bgtile_attr = 0;

    clear_screen();
}

void ppu_step(int clock_budget)
{
    // run as many cycles as the budget allows
    for (int clocks = 0; clocks < clock_budget; clocks++) {
        // free cycle on oddframe
        if (cycle == 0 && oddframe) {
            clocks--;
        }

        if (cycle == 1 && scanline == 241) {
            ppustatus.field.vblank = 1;
            if (ppuctrl.field.nmi_gen) {
                cpu_nmi();
            }
        }

        if (cycle == 1 && scanline == 261) {
            ppustatus.field.vblank = 0;
            // TODO sprite 0 stuff, overflow...
        }

        if (in_render_zone()) {
            // update the shifter registers
            shift_bgshifters();
            

            // 8 cycle fetch pattern
            u16 addr, hi;
            switch ((cycle - 1) % 8) {
            case 0:
                load_bgshifters();
                
                // fetch nt tile addr (only 12 bits needed from ppuaddr)
                addr = 0x2000 | (ppuaddr.raw & 0x0FFF);
                nx_bgtile_id = ppu_read(addr);
                break;
            case 2:
                // fetch tile attribute
                addr = 0x23C0; // base location of attributes
                addr |= (ppuaddr.field.x_nt << 11);
                addr |= (ppuaddr.field.y_nt << 10);
                // course x/y only need 3 msb
                addr |= ((ppuaddr.field.course_y >> 2) << 3);
                addr |= (ppuaddr.field.course_x >> 2);
                nx_bgtile_attr = ppu_read(addr);

                // tiles are in 2x2 chunks so next we figure out which tile we need
                if (ppuaddr.field.course_y & 0x02) { // top half
                    nx_bgtile_attr >>= 4;
                }
                if (ppuaddr.field.course_x & 0x02) { // left half
                    nx_bgtile_attr >>= 2;
                }
                nx_bgtile_attr &= 0x03; // we only need 2 bits to index
                break;
            case 4:
                // fetch lsb of next tile
                addr = ppuctrl.field.bg_side << 12;
                addr += (nx_bgtile_id << 4);
                addr += ppuaddr.field.fine_y;
                nx_bgtile = ppu_read(addr);
                break;
            case 6:
                // fetch msb of next tile
                addr = ppuctrl.field.bg_side << 12;
                addr += (nx_bgtile_id << 4);
                addr += ppuaddr.field.fine_y + 8; // offset one byte
                hi = ppu_read(addr);
                nx_bgtile |= (hi << 8);
                break;
            case 7:
                scroll_horz();
                break;
            }

            render_px();
        }

        if (cycle == 256) {
            scroll_vert();
        }

        if (cycle == 257) {
            reset_horz();
        }

        if (cycle >= 280 && cycle <= 305 && scanline == 261) {
            reset_vert();
        }


        // increase screen state
        cycle = (cycle + 1) % NUM_CYCLES;
        if (cycle == 0) {
            scanline = (scanline + 1) % NUM_SCANLINES;
            if (scanline == 0) {
                oddframe = !oddframe;
            }
        }
    }
}

u8 ppu_reg_read(u16 reg)
{
    u8 data = 0;
    switch (reg) {
    case 0: // PPUCTRL
        // no read access
        #ifdef DEBUG
        data = ppuctrl.raw;
        #endif
        break;
    case 1: // PPUMASK
        // no read access
        #ifdef DEBUG
        data = ppumask.raw;
        #endif
        break;
    case 2: // PPUSTATUS
        data = ppustatus.raw;
        ppustatus.field.vblank = 0;
        al_first_write = true;
        break;
    case 3: // OAMADDR
        // no read access
        #ifdef DEBUG
        data = oamaddr;
        #endif
        break;
    case 4: // OAMDATA
        data = oam[oamaddr];
        break;
    case 5: // PPUSCROLL
        // no read access
        #ifdef DEBUG
        data = ppuaddr.raw;
        #endif
        break;
    case 6: // PPUADDR
        // no read access
        #ifdef DEBUG
        data = ppuaddr.raw;
        #endif
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
