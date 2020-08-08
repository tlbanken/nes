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

#define LOG(fmt, ...) neslog(LID_PPU, fmt, ##__VA_ARGS__);
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
static bool is_init = false;

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
static u8 sprite_shifters[8];
static u8 sprite_attrs[8];
static u8 sprite_counters[8];

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

//---------------------------------------------------------------
// PPU Debug Display
//---------------------------------------------------------------
void draw_pattern_table(u16 table_id, u8 pal_id)
{
    for (u16 ytile = 0; ytile < 16; ytile++) {
        for (u16 xtile = 0; xtile < 16; xtile++) {
            // convert 2D indexing to 1D indexing
            u16 byte_offset = (ytile * 256) + (xtile * 16);

            // Now iterate over the bytes in a tile and then each bit in the byte
            for (u16 row = 0; row < 8; row++) {
                u16 addr = table_id * 0x1000 + byte_offset + row;
                u8 tile_lsb = ppu_read(addr);
                u8 tile_msb = ppu_read(addr + 8);

                for (u16 col = 0; col < 8; col++) {
                    u8 px = (tile_lsb & 0x1) + (tile_msb & 0x1);
                    // shift tile byte
                    tile_lsb >>= 1;
                    tile_msb >>= 1;

                    u16 x = (7 - col) + (xtile * 8);
                    u16 y = row + (ytile * 8);

                    u8 color_id = ppu_read(0x3F00 + (pal_id << 2) + px);
                    nes_color_t color = nes_colors[color_id];

                    set_px_pt(table_id, x, y, color);
                }
            }
        }
    }
}

//---------------------------------------------------------------
// PPU Helpers
//---------------------------------------------------------------
static inline bool in_visible_scanlines()
{
    return scanline < 240 || scanline == 261;
}

static void render_px()
{
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
        u8 col_id = ppu_read(addr) % 64;
        nes_color_t col = nes_colors[col_id];

        // draw the pixel
        set_px(cycle, scanline, col);
    }
}

// increment course_x as described here:
// https://wiki.nesdev.com/w/index.php/PPU_scrolling
static void incr_horz()
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
static void incr_vert()
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
        ppuaddr.field.course_y = ppuaddr_tmp.field.course_y;
        ppuaddr.field.y_nt = ppuaddr_tmp.field.y_nt;
        ppuaddr.field.fine_y = ppuaddr_tmp.field.fine_y;
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

// Occurs on cycles 257-320
static void sprite_fetch()
{
    switch ((cycle - 257) % 8) {
    case 0:
        // read y coord from OAM buf
        break;
    case 1:
        // read tile num from OAM buf
        break;
    case 2:
        // read attr from OAM buf
        break;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        // read x coord from OAM buf
        break;
    default:
        ERROR("Out of sync cycle number\n");
        EXIT(1);
    }
}

// https://wiki.nesdev.com/w/index.php/PPU_sprite_evaluation
// Occurs on cycles 65-256
static void sprite_evaluation()
{
    static bool oambuf_full = false;
    static u32 oambuf_index = 0;
    static u32 oam_index = 0;
    static u8 buffer = 0;
    static bool sprite_found = false;

    // check if rendering is on
    if (!ppumask.field.render_bg && !ppumask.field.render_sprites) {
        return;
    }

    // reset
    if (cycle == 65) {
        oam_index = 0;
        oambuf_index = 0;
        oambuf_full = false;
        sprite_found = false;
        buffer = 0;
    }

    if (oambuf_full) {
        return;
    }

    // Odd Cycles: Read from OAM
    // Even Cycles: Write to OAM Buffer
    if (cycle % 2 == 1) {
        buffer = oam[oam_index];
        // last read
        if (oam_index % 4 == 3) {
            oambuf[oambuf_index] = buffer;
            oambuf_index++;
            sprite_found = false;
        }
        // read y coord
        if (oam_index % 4 == 0) {
            // check if sprite belongs on this scanline
            if (buffer == scanline) {
                ppustatus.field.sprite_overflow = 1;
                sprite_found = true;
            } else {
                // skip sprite
                oam_index += 3;
                // due to bug increment again
                oam_index++;
            }
        } 
        oam_index++;
    } else {
        if (sprite_found) {
            // set sprite overflow flag?
            oambuf[oambuf_index] = buffer;
            oambuf_index++;
        }
    }

    // handle overflows
    if (oambuf_index >= 32) {
        oambuf_index = 0;
        oambuf_full = true;
    }
    if (oam_index >= 256) {
        oam_index = 0;
        // TODO
    }
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
    is_init = true;
}

bool ppu_step(int clock_budget)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    bool frame_finished = false;
    // run as many cycles as the budget allows
    for (int clocks = 0; clocks < clock_budget; clocks++) {
        if (!in_visible_scanlines()) {
            goto post_visible_scanlines;
        }

        // free cycle on oddframe
        if (cycle == 0 && oddframe && scanline == 0) {
            cycle = 1;
        }

        // LOG("CY:%03u SL:%03u OD:%u    C:%02X M:%02X S:%02X V:%04X T:%04X    RENDER: %u\n",
        //     cycle, scanline, oddframe, ppuctrl.raw, ppumask.raw, ppustatus.raw,
        //     ppuaddr.raw, ppuaddr_tmp.raw, in_visible_scanlines());

        if ((cycle >= 1 && cycle <= 257) || (cycle >= 321 && cycle <= 340)) {
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
                addr |= (ppuaddr.field.y_nt << 11);
                addr |= (ppuaddr.field.x_nt << 10);
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
                incr_horz();
                break;
            }

        }

        if (cycle >= 1 && cycle <= 64) {
            // TODO fill oam buffer with 0xFF
        }

        if (cycle >= 65 && cycle <= 256) {
            sprite_evaluation();
        }

        if (cycle >= 257 && cycle <= 320) {
            sprite_fetch();
        }

        if (cycle == 1 && scanline == 261) {
            ppustatus.field.vblank = 0;
            // TODO sprite 0 stuff, overflow...
        }

        if (cycle == 256) {
            incr_vert();
        }

        if (cycle == 257) {
            load_bgshifters();
            reset_horz();
        }

        if (cycle >= 280 && cycle <= 304 && scanline == 261) {
            reset_vert();
        }

        // ********************************
        // Out of visible scanline region
        // ********************************
post_visible_scanlines:
        if (cycle == 1 && scanline == 241) {
            ppustatus.field.vblank = 1;
            if (ppuctrl.field.nmi_gen) {
                cpu_nmi();
            }
        }

        render_px();
        // increase screen state
        cycle = (cycle + 1) % NUM_CYCLES;
        if (cycle == 0) {
            scanline = (scanline + 1) % NUM_SCANLINES;
            if (scanline == 0) {
                oddframe = !oddframe;
                frame_finished = true;
            }
        }
    }
    return frame_finished;
}

u8 ppu_reg_read(u16 reg)
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
        if (cycle >= 1 && cycle <= 64 && in_visible_scanlines()) {
            data = 0xFF;
        } else {
            data = oam[oamaddr];
        }
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
#ifdef DEBUG
    CHECK_INIT;
#endif
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
            // ppuaddr_tmp.field.fine_y &= 0x3; // turn off bit 14
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
#ifdef DEBUG
    CHECK_INIT;
#endif
    for (u16 lo = 0; lo < 256; lo++) {
        u16 addr = ((u16)hi) << 8;
        addr |= lo;
        u8 val = cpu_read(addr);
        oam[oamaddr] = val;
        oamaddr++;
    }
}

void ppu_dump()
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
    fprintf(ofile, "$2007 (PPUADDR)   = %04X\n", ppuaddr.raw);
    fprintf(ofile, "   course_x: %u\n", ppuaddr.field.course_x);
    fprintf(ofile, "   course_y: %u\n", ppuaddr.field.course_y);
    fprintf(ofile, "   x_nt    : %u\n", ppuaddr.field.x_nt);
    fprintf(ofile, "   y_nt    : %u\n", ppuaddr.field.y_nt);
    fprintf(ofile, "   fine_y  : %u\n", ppuaddr.field.fine_y);
    fprintf(ofile, "\n");
    fprintf(ofile, "$2007 (PPUADDR_TEMP)   = %04X\n", ppuaddr_tmp.raw);
    fprintf(ofile, "   course_x: %u\n", ppuaddr_tmp.field.course_x);
    fprintf(ofile, "   course_y: %u\n", ppuaddr_tmp.field.course_y);
    fprintf(ofile, "   x_nt    : %u\n", ppuaddr_tmp.field.x_nt);
    fprintf(ofile, "   y_nt    : %u\n", ppuaddr_tmp.field.y_nt);
    fprintf(ofile, "   fine_y  : %u\n", ppuaddr_tmp.field.fine_y);
    fprintf(ofile, "---------------------------------------\n");
    fclose(ofile);
}
