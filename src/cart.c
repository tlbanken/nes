/*
 * cart.c
 *
 * Travis Banken
 * 2020
 *
 * The Cartridge handlers.
 */

#include <string.h>

#include <cart.h>
#include <mem.h>

#define CHECK_INIT if(!is_init){ERROR("Not Initialized!\n"); EXIT(1);}

#define INES_HEADER_SIZE 16

// iNES as describe from nes dev
// https://wiki.nesdev.com/w/index.php/INES
typedef struct ines_header {
    char str_nes[4]; // should contain "NES"
    char magic_1A;   // MS-DOS end-of-file
    u8 prgrom_size;  // Bank Size in 16 KB units
    u8 chrrom_size;  // Bank Size in 8 KB units
    struct {
        u8 mirror_mode: 1;   // 0: horz, 1: vert
        u8 battery: 1;       // 0: No battey backed rom, 1: yes battery
        u8 trainer: 1;       // 0: no trainer, 1: yes trainer
        u8 fourscreen_mir: 1;// 0: no 4 screen mirroring, 1: ignore mirror mode and use 4 screen mirror
        u8 vs_unisys: 1;
        u8 playchoice_10: 1; // 8KB of Hint Screen data stored after CHR data
        u8 nes2: 2;          // if == 2 then flags 8-15 in NES 2.0 format
    } flags;
    u8 mapper_num;
    u8 prgram_size; // PRG RAM size in 8KB units

    // Some more stuff but its not important :/
} ines_header_t;
static ines_header_t inesh;

static size_t prgrom_size;
static size_t chrrom_size;

static ines_header_t read_ines_header(FILE *file)
{
    assert(file != NULL);

    u8 filebuf[INES_HEADER_SIZE];
    int bytes = fread(filebuf, 1, INES_HEADER_SIZE, file);
    assert(bytes == INES_HEADER_SIZE);

    // fill ines header
    int i = 0;
    ines_header_t header;
    memcpy(header.str_nes, filebuf, 3);
    header.str_nes[3] = '\0';
    i += 3;
    header.magic_1A = filebuf[i++];
    header.prgrom_size = filebuf[i++];
    header.chrrom_size = filebuf[i++];

    u8 flags6, flags7;
    flags6 = filebuf[i++];
    flags7 = filebuf[i++];
    // fill in flags
    header.flags.mirror_mode    = (flags6 >> 0) & 0x1;
    header.flags.battery        = (flags6 >> 1) & 0x1;
    header.flags.trainer        = (flags6 >> 2) & 0x1;
    header.flags.fourscreen_mir = (flags6 >> 3) & 0x1;
    header.flags.vs_unisys      = (flags7 >> 0) & 0x1;
    header.flags.playchoice_10  = (flags7 >> 1) & 0x1;
    header.flags.nes2           = (flags7 >> 2) & 0x3;
    // fill in mapper num
    header.mapper_num = (flags6 >> 4) & 0xF;
    header.mapper_num |= (flags7 & 0xF0);
    // fill in rest of header
    header.mapper_num = filebuf[i++];
    header.prgram_size = filebuf[i++];

    return header;
}

// *** Mappers ***
static u16 cpu_map000(u16 addr)
{
    // TODO figure out what this is?
    if (addr < 0x6000) {
        WARNING("Trying to access unsupported address ($%04X)\n", addr);
        return addr;
    }

    // PRG-RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        return addr;
    }

    // PRG-ROM
    if (addr >= 0x8000) {
        return ((addr - 0x8000) % prgrom_size) + 0x8000;
    }

    // should not get here
    assert(0);
    return 0;
}
static u16 ppu_map000(u16 addr)
{
    return addr;
}

// *** END Mappers ***
static bool is_init = false;
void Cart_Init(const char *path)
{
    is_init = true;
}

void Cart_Load(const char *path)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    FILE *romfile = fopen(path, "rb");
    if (romfile == NULL) {
        perror("fopen");
        ERROR("Failed to open %s\n", path);
        EXIT(1);
    }

    inesh = read_ines_header(romfile);
    if (inesh.flags.trainer) {
        // TODO add trainer support ???
        ERROR("No trainer support :(\n");
        EXIT(1);
    }

    prgrom_size = inesh.prgrom_size * (16*1024);
    assert(prgrom_size != 0);
    chrrom_size = inesh.chrrom_size * (8*1024);
    // assert(chrrom_size != 0);

    // write out prg rom
    u8 prgrom_buf[prgrom_size];
    fread(prgrom_buf, 1, prgrom_size, romfile);
    for (u32 i = 0; i < prgrom_size; i++) {
        Mem_CpuWrite(prgrom_buf[i], i + 0x8000);
    }

    u8 chrrom_buf[chrrom_size];
    fread(chrrom_buf, 1, chrrom_size, romfile);
    for (u32 i = 0; i < chrrom_size; i++) {
        Mem_PpuWrite(chrrom_buf[i], i);
    }

    // TODO the rare extensions

    fclose(romfile);
}

u16 Cart_CpuMap(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    switch (inesh.mapper_num) {
    case 0:
        return cpu_map000(addr);
    default:
        ERROR("Mapper (%u) not supported!", inesh.mapper_num);
        EXIT(1);
    }
    // should not get here
    assert(0);
    return 0;
}

u16 Cart_PpuMap(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    switch (inesh.mapper_num) {
    case 0:
        return ppu_map000(addr);
    default:
        ERROR("Mapper (%u) not supported!", inesh.mapper_num);
        EXIT(1);
    } 
    // should not get here
    assert(0);
    return 0;
}

inline enum mirror_mode Cart_GetMirrorMode()
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    // first check if four screen mode on
    if (inesh.flags.fourscreen_mir) {
        return MIR_4SCRN;
    }

    if (inesh.flags.mirror_mode) {
        return MIR_HORZ;
    } else {
        return MIR_VERT;
    }
    // should not get here
    assert(0);
    return 0;
}

void Cart_Dump()
{
#ifdef DEBUG
    if (!is_init) {
        WARNING("Not Initialized!\n");
    }
#endif
    // dump iNES header read
    FILE *ofile = fopen("ines.dump", "w");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump iNES\n");
        return;
    }
    fprintf(ofile, "---------------------------------------\n");
    fprintf(ofile, "iNES Header Dump\n");
    fprintf(ofile, "---------------------------------------\n");
    fprintf(ofile, "NES STR: %s\n", inesh.str_nes);
    fprintf(ofile, "Magic 1A: %02X\n", inesh.magic_1A);
    fprintf(ofile, "Num PRG-ROM Banks: %u\n", inesh.prgrom_size);
    fprintf(ofile, "Num CHR-ROM Banks: %u\n", inesh.chrrom_size);
    fprintf(ofile, "*** Flags ***\n");
    fprintf(ofile, "    Mirror Type: %u\n", inesh.flags.mirror_mode);
    fprintf(ofile, "    Battery: %u\n", inesh.flags.battery);
    fprintf(ofile, "    Trainer: %u\n", inesh.flags.trainer);
    fprintf(ofile, "    4 Screen Mirror: %u\n", inesh.flags.fourscreen_mir);
    fprintf(ofile, "    VS Unisystem: %u\n", inesh.flags.vs_unisys);
    fprintf(ofile, "    PlayChoice-10: %u\n", inesh.flags.playchoice_10);
    fprintf(ofile, "    NES 2.0: %u\n", inesh.flags.nes2);
    fprintf(ofile, "Mapper Num: %u\n", inesh.mapper_num);
    fprintf(ofile, "Num Ram Banks: %u\n", inesh.prgram_size);
    fprintf(ofile, "---------------------------------------\n");
    fclose(ofile);
}