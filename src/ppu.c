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
// Object Attrubute Memory (sprites)
static u8 oam[256] = {0};
// Sprite Struct
typedef struct sprite {
    u8 ypos;
    u8 xpos;
    // 8x8 sprites: Tile number within pattern table selected from PPUCTRL
    // 8x16 sprites: selects pattern table from bit 0
    struct {
        u8 bank: 1;     // 0: $0000, 1: $1000 (only in 8x16 mode)
        u8 tile_num: 7; // tile num for top of sprite (bottom half gets next tile)
    } index;
    // Attributes
    struct {
        u8 palette: 2;  // palette for the sprite 
        u8 unused: 3;
        u8 priority: 1; // 0: in front of background, 1: behind
        u8 flip_h: 1;   // 0: no flip, 1: flip
        u8 flip_v: 1;   // 0: no flip, 1: flip
    } attr;
} sprite_t;
// Secondary OAM (holds 8 sprites)
static u8 oambuf[8*4];

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
        u16 coarse_x: 5;
        u16 coarse_y: 5;
        u16 x_nt: 1;
        u16 y_nt: 1;
        u16 fine_y: 3;
        u16 unused: 1;
    } field;
    u16 raw;
} loopyreg_t;
static loopyreg_t loopy_v;
static loopyreg_t loopy_t;
static u8 fine_x;

// other state vars
static bool al_first_write = true;
static u8 ppudata_buf;

// screen state
#define NUM_CYCLES 341
#define NUM_SCANLINES 262
static int cycle;
static int scanline;
static bool oddframe = false;

// bg shifters
static u16 bgshifter_ptrn_lo;
static u16 bgshifter_ptrn_hi;
static u16 bgshifter_attr_lo;
static u16 bgshifter_attr_hi;

// sprite shifters
// static u8 sprite_shifters[8];
// static u8 sprite_attrs[8];
// static u8 sprite_counters[8];

// tile buffers
static u8 nx_bgtile_id;
static u16 nx_bgtile;
static u8 nx_bgtile_attr;

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

static void render_px()
{
    // no point in rendering if in vblank
    if (ppustatus.field.vblank) {
        return;
    }

    if (ppumask.field.render_bg) {
        u16 fine_bit = 0x8000 >> fine_x;
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
        u8 col_id = Mem_PpuRead(addr) & 0x3F;
        nes_color_t col = nes_colors[col_id];
        // TODO: Add in color emphasis and greyscale

        // NOTE: Debug
        // if (scanline == 140) {
        //     col.red = 255;
        //     col.blue = 0;
        //     col.blue = 0;
        // }

        // draw the pixel
        Vac_SetPx(cycle, scanline, col);
    } else if (loopy_v.raw >= 0x3F00 && loopy_v.raw <= 0x3FFF) {
        // pallete hack
        // https://wiki.nesdev.com/w/index.php/PPU_palettes
        u8 col_id = Mem_PpuRead(loopy_v.raw) & 0x3F;
        nes_color_t col = nes_colors[col_id];
        Vac_SetPx(cycle, scanline, col);
    }
}

static void inc_hori()
{
    if (ppumask.field.render_bg || ppumask.field.render_sprites) {
        if (loopy_v.field.coarse_x == 31) {
            loopy_v.field.coarse_x = 0;
            loopy_v.field.x_nt = !loopy_v.field.x_nt;
        } else {
            loopy_v.field.coarse_x++;
        }
    }
}

static void inc_vert()
{
    // check that rendering is on, then increment y
    if (ppumask.field.render_bg || ppumask.field.render_sprites) {
        // increment fine_y until overflow
        if (loopy_v.field.fine_y < 7) {
            loopy_v.field.fine_y += 1;
        } else {
            loopy_v.field.fine_y = 0;
            if (loopy_v.field.coarse_y == 29) {
                loopy_v.field.coarse_y = 0;
                loopy_v.field.y_nt = !loopy_v.field.y_nt;
            } else if (loopy_v.field.coarse_y == 31) {
                loopy_v.field.coarse_y = 0;
            } else {
                loopy_v.field.coarse_y++;
            }
        }
    }
}

static void shift_bgshifters()
{
    if (ppumask.field.render_bg) {
        bgshifter_ptrn_lo <<= 1;
        bgshifter_ptrn_hi <<= 1;
        bgshifter_attr_lo <<= 1;
        bgshifter_attr_hi <<= 1;
    }
}

static void load_bgshifters()
{
    // Every cycle we use the lsb of the pttrn and attr shifters to render the
    // pixel. Here we load the future byte into the msb of the 16-bit shifters

    // pattern bits
    bgshifter_ptrn_lo = (bgshifter_ptrn_lo & 0xFF00) | (nx_bgtile & 0xFF);
    bgshifter_ptrn_hi = (bgshifter_ptrn_hi & 0xFF00) | (nx_bgtile >> 8);

    // attribute bits
    bgshifter_attr_lo = (bgshifter_attr_lo & 0xFF00) | (nx_bgtile_attr & 0x1 ? 0xFF : 0x00);
    bgshifter_attr_hi = (bgshifter_attr_hi & 0xFF00) | (nx_bgtile_attr & 0x2 ? 0xFF : 0x00);
}

void Ppu_Init()
{
    // sanity check union-struct hacks
    assert(sizeof(reg_ppuctrl_t) == 1);
    assert(sizeof(reg_ppumask_t) == 1);
    assert(sizeof(reg_ppustatus_t) == 1);
    assert(sizeof(loopyreg_t) == 2);

    is_init = true;
}

void Ppu_Reset()
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    
    // setup initial state
    cycle = 0;
    scanline = 0;
    al_first_write = true;
    ppudata_buf = 0;
    oddframe = false;

    ppuctrl.raw = 0;
    ppumask.raw = 0;
    ppustatus.raw = 0;
    oamaddr = 0;

    loopy_v.raw = 0;
    loopy_t.raw = 0;

    fine_x = 0;

    bgshifter_ptrn_lo = 0;
    bgshifter_ptrn_hi = 0;
    bgshifter_attr_lo = 0;
    bgshifter_attr_hi = 0;
    nx_bgtile_id = 0;
    nx_bgtile_attr = 0;
}

bool Ppu_Step(int clock_budget)
{
#ifdef DEBUG
    CHECK_INIT;
#endif

    bool frame_finished = false;
    for (int clocks = 0; clocks < clock_budget; clocks++) {

        // LOG("CY:%03u SL:%03u OD:%u    C:%02X M:%02X S:%02X V:%04X T:%04X\n",
        //     cycle, scanline, oddframe, ppuctrl.raw, ppumask.raw, ppustatus.raw,
        //     loopy_v.raw, loopy_t.raw);

        // free cycle on oddframes
        if (scanline == 0 && cycle == 0 && oddframe) {
            cycle = 1;
        }

        // Visible Scanlines
        if (scanline <= 239 || scanline == 261) {

            // clear vblank
            if (scanline == 261 && cycle == 1) {
                ppustatus.field.vblank = 0;
                // TODO: Sprite overflow
            }

            if ((cycle >= 1 && cycle <= 256) || cycle >= 321) {
                shift_bgshifters();

                u16 addr, hi;
                // prepare next value to be loaded into shifter
                switch ((cycle - 1) % 8) {
                case 0:
                    // fetch nametable byte
                    nx_bgtile_id = Mem_PpuRead(0x2000 | (loopy_v.raw & 0xFFF));
                    break;
                case 2:
                    // fetch attribute table byte
                    addr = 0x23C0; // base location of attributes
                    addr |= (loopy_v.field.y_nt << 11);
                    addr |= (loopy_v.field.x_nt << 10);
                    // course x/y only need 3 msb
                    addr |= ((loopy_v.field.coarse_y >> 2) << 3);
                    addr |= (loopy_v.field.coarse_x >> 2);
                    nx_bgtile_attr = Mem_PpuRead(addr);

                    // tiles are in 2x2 chunks so next we figure out which tile we need
                    if (loopy_v.field.coarse_y & 0x02) { // top half
                        nx_bgtile_attr >>= 4;
                    }
                    if (loopy_v.field.coarse_x & 0x02) { // left half
                        nx_bgtile_attr >>= 2;
                    }
                    nx_bgtile_attr &= 0x03; // we only need 2 bits to index
                    break;
                case 4:
                    // fetch lsb of next tile
                    addr = ppuctrl.field.bg_side << 12;
                    addr += (nx_bgtile_id << 4);
                    addr += loopy_v.field.fine_y;
                    nx_bgtile = Mem_PpuRead(addr);
                    break;
                case 6:
                    // fetch msb of next tile
                    addr = ppuctrl.field.bg_side << 12;
                    addr += (nx_bgtile_id << 4);
                    addr += loopy_v.field.fine_y + 8; // offset one byte
                    hi = Mem_PpuRead(addr);
                    nx_bgtile |= (hi << 8);
                    break;
                case 7:
                    inc_hori();
                    load_bgshifters();
                    break;
                }
            }

            if (cycle == 257) {
                load_bgshifters();
                // reset horizontal loopy registers
                if (ppumask.field.render_bg || ppumask.field.render_sprites) {
                    loopy_v.field.coarse_x = loopy_t.field.coarse_x;
                    loopy_v.field.x_nt     = loopy_t.field.x_nt;
                }
            } else if (cycle == 256) {
                // increment vertical loopy
                inc_vert();
            } else if (scanline == 261 && (cycle >= 280 && cycle <= 304)) {
                // reset vertical loopy registers
                if (ppumask.field.render_bg || ppumask.field.render_sprites) {
                    loopy_v.field.coarse_y = loopy_t.field.coarse_y;
                    loopy_v.field.y_nt     = loopy_t.field.y_nt;
                    loopy_v.field.fine_y   = loopy_t.field.fine_y;
                }
            }

        } else { // NonVisible Scanlines

            // set vblank flag
            if (scanline == 241 && cycle == 1) {
                ppustatus.field.vblank = 1;
                if (ppuctrl.field.nmi_gen) {
                    Cpu_Nmi();
                }
            }
        }

        render_px();
        // Update Screen State
        if (cycle == 340) {
            cycle = 0;
            if (scanline == 261) {
                scanline = 0;
                oddframe = !oddframe;
                frame_finished = true;
            } else {
                scanline++;
            }
        } else {
            cycle++;
        }
    }
    return frame_finished;
}

u8 Ppu_RegRead(u16 reg)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
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
        // if (cycle >= 1 && cycle <= 64 && in_visible_scanlines()) {
        //     data = 0xFF;
        // } else {
        //     data = oam[oamaddr];
        // }
        // TODO
        break;
    case 5: // PPUSCROLL
        // no read access
        #ifdef DEBUG
        data = loopy_v.raw;
        #endif
        break;
    case 6: // PPUADDR
        // no read access
        #ifdef DEBUG
        data = loopy_v.raw;
        #endif
        break;
    case 7: // PPUDATA
        data = ppudata_buf;
        ppudata_buf = Mem_PpuRead(loopy_v.raw);
        if (loopy_v.raw >= 0x3F00) { // no delay from CHR-ROM
            data = ppudata_buf;
        }
        loopy_v.raw += (ppuctrl.field.vram_incr ? 32 : 1);
        break;
    default:
        ERROR("Unknown ppu register (%u)\n", reg);
        EXIT(1);
    }
    return data;
}

void Ppu_RegWrite(u8 val, u16 reg)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    switch (reg) {
    case 0: // PPUCTRL
        ppuctrl.raw = val;
        loopy_t.field.x_nt = ppuctrl.field.x_nt;
        loopy_t.field.y_nt = ppuctrl.field.y_nt;
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
            loopy_t.field.coarse_x = (val >> 3);
            fine_x = (val & 0x7);
        } else {
            loopy_t.field.coarse_y = (val >> 3);
            loopy_t.field.fine_y = (val & 0x7);
        }
        al_first_write = !al_first_write;
        break;
    case 6: // PPUADDR
        if (al_first_write) {
            loopy_t.raw = (((u16)val & 0x3F) << 8) | (loopy_t.raw & 0x00FF);
        } else {
            loopy_t.raw = (loopy_t.raw & 0xFF00) | val;
            loopy_v.raw = loopy_t.raw; // copy over temp to cur
        }
        al_first_write = !al_first_write;
        break;
    case 7: // PPUDATA
        Mem_PpuWrite(val, loopy_v.raw);
        loopy_v.raw += (ppuctrl.field.vram_incr ? 32 : 1);
        break;
    default:
        ERROR("Unknown ppu register (%u)\n", reg);
        EXIT(1);
    }
    return;
}

void Ppu_Oamdma(u8 hi)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    for (u16 lo = 0; lo < 256; lo++) {
        u16 addr = ((u16)hi) << 8;
        addr |= lo;
        u8 val = Mem_CpuRead(addr);
        oam[oamaddr] = val;
        oamaddr++;
    }
}

//---------------------------------------------------------------
// PPU Debug Display
//---------------------------------------------------------------
void Ppu_DrawPT(u16 table_id, u8 pal_id)
{
    for (u16 ytile = 0; ytile < 16; ytile++) {
        for (u16 xtile = 0; xtile < 16; xtile++) {
            // convert 2D indexing to 1D indexing
            u16 byte_offset = (ytile * 256) + (xtile * 16);

            // Now iterate over the bytes in a tile and then each bit in the byte
            for (u16 row = 0; row < 8; row++) {
                u16 addr = table_id * 0x1000 + byte_offset + row;
                u8 tile_lsb = Mem_PpuRead(addr);
                u8 tile_msb = Mem_PpuRead(addr + 8);

                for (u16 col = 0; col < 8; col++) {
                    u8 px = (tile_lsb & 0x1) + (tile_msb & 0x1);
                    // shift tile byte
                    tile_lsb >>= 1;
                    tile_msb >>= 1;

                    u16 x = (7 - col) + (xtile * 8);
                    u16 y = row + (ytile * 8);

                    u8 color_id = Mem_PpuRead(0x3F00 + (pal_id << 2) + px);
                    nes_color_t color = nes_colors[color_id & 0x3F];

                    Vac_SetPxPt(table_id, x, y, color);
                }
            }
        }
    }
}


void Ppu_Dump()
{
#ifdef DEBUG
    if (!is_init) {
        WARNING("Not Initialized!\n");
    }
#endif
    FILE *ofile = fopen("ppu.dump", "w");
    if (ofile == NULL) {
        perror("fopen");
        return;
    }
    fprintf(ofile, "---------------------------------------\n");
    fprintf(ofile, "PPU REGS\n");
    fprintf(ofile, "---------------------------------------\n");
    fprintf(ofile, "$2000 (PPUCTRL)   = %02X\n", ppuctrl.raw);
    fprintf(ofile, "   x_nt        : %u\n", ppuctrl.field.x_nt);
    fprintf(ofile, "   y_nt        : %u\n", ppuctrl.field.y_nt);
    fprintf(ofile, "   vram_incr   : %u\n", ppuctrl.field.vram_incr);
    fprintf(ofile, "   sprite_side : %u\n", ppuctrl.field.sprite_side);
    fprintf(ofile, "   bg_side     : %u\n", ppuctrl.field.bg_side);
    fprintf(ofile, "   sprite_size : %u\n", ppuctrl.field.sprite_size);
    fprintf(ofile, "   master_slave: %u\n", ppuctrl.field.master_slave);
    fprintf(ofile, "   nmi_gen     : %u\n", ppuctrl.field.nmi_gen);
    fprintf(ofile, "\n");
    fprintf(ofile, "$2001 (PPUMASK)   = %02X\n", ppumask.raw);
    fprintf(ofile, "   greyscale      : %u\n", ppumask.field.greyscale);
    fprintf(ofile, "   render_lbg     : %u\n", ppumask.field.render_lbg);
    fprintf(ofile, "   render_lsprites: %u\n", ppumask.field.render_lsprites);
    fprintf(ofile, "   render_bg      : %u\n", ppumask.field.render_bg);
    fprintf(ofile, "   render_sprites : %u\n", ppumask.field.render_sprites);
    fprintf(ofile, "   emph_red       : %u\n", ppumask.field.emph_red);
    fprintf(ofile, "   emph_green     : %u\n", ppumask.field.emph_green);
    fprintf(ofile, "   emph_blue      : %u\n", ppumask.field.emph_blue);
    fprintf(ofile, "\n");
    fprintf(ofile, "$2002 (PPUSTATUS) = %02X\n", ppustatus.raw);
    fprintf(ofile, "   sprite_overflow: %u\n", ppustatus.field.sprite_overflow);
    fprintf(ofile, "   sprite0_hit    : %u\n", ppustatus.field.sprite0_hit);
    fprintf(ofile, "   vblank         : %u\n", ppustatus.field.vblank);
    fprintf(ofile, "\n");
    fprintf(ofile, "$2003 (OAMADDR)   = %02X\n", oamaddr);
    fprintf(ofile, "\n");
    fprintf(ofile, "$2007 (PPUADDR)   = %04X\n", loopy_v.raw);
    fprintf(ofile, "   coarse_x: %u\n", loopy_v.field.coarse_x);
    fprintf(ofile, "   coarse_y: %u\n", loopy_v.field.coarse_y);
    fprintf(ofile, "   x_nt    : %u\n", loopy_v.field.x_nt);
    fprintf(ofile, "   y_nt    : %u\n", loopy_v.field.y_nt);
    fprintf(ofile, "   fine_y  : %u\n", loopy_v.field.fine_y);
    fprintf(ofile, "\n");
    fprintf(ofile, "$2007 (PPUADDR_TEMP)   = %04X\n", loopy_t.raw);
    fprintf(ofile, "   coarse_x: %u\n", loopy_t.field.coarse_x);
    fprintf(ofile, "   coarse_y: %u\n", loopy_t.field.coarse_y);
    fprintf(ofile, "   x_nt    : %u\n", loopy_t.field.x_nt);
    fprintf(ofile, "   y_nt    : %u\n", loopy_t.field.y_nt);
    fprintf(ofile, "   fine_y  : %u\n", loopy_t.field.fine_y);
    fprintf(ofile, "---------------------------------------\n");
    fclose(ofile);
}
