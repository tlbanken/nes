/*
 * cart.c
 *
 * Travis Banken
 * 2020
 *
 * The Cartridge handlers.
 */

#include <string.h>
#include <stdlib.h>

#include <cart.h>
#include <mem.h>
#include <mappers.h>

#define CHECK_INIT if(!is_init){ERROR("Not Initialized!\n"); EXIT(1);}

#define INES_HEADER_SIZE 16

// iNES as describe from nes dev
// https://wiki.nesdev.com/w/index.php/INES
typedef struct ines_header {
    u8 prgrom_banks;  // Bank Size in 16 KB units
    u8 chrrom_banks;  // Bank Size in 8 KB units
    u8 prgram_banks;  // PRG RAM size in 8KB units
    bool battery;     // battery-backed ram present
    bool trainer;     // 512-byte trainer present
    u8 mapper_num;    // Cartridge Mapper number
    enum mirror_mode mirror_mode;

    // Some more stuff but its not important :/
} ines_header_t;
static ines_header_t inesh;

// cartridge memory (dynamic memory)
static u8 *cartmem = NULL;
static size_t cartmem_size = 0;
static u8 *chrrom = NULL;
static size_t chrrom_size = 0;
// static u8 cartmem[0xBFE0] = {0};
// static u8 chrrom[(8*1024)] = {0};

static ines_header_t read_ines_header(FILE *file)
{
    assert(file != NULL);

    u8 filebuf[INES_HEADER_SIZE];
    int bytes = fread(filebuf, 1, INES_HEADER_SIZE, file);
    assert(bytes == INES_HEADER_SIZE);

    // fill ines header
    int i = 0;
    ines_header_t header;

    // skip over first 3 bytes (not important)
    i += 3;

    // check magic number
    u8 magic_1A = filebuf[i++];
    assert(magic_1A == 0x1A);

    header.prgrom_banks = filebuf[i++];
    header.chrrom_banks = filebuf[i++];

    u8 flags6, flags7;
    flags6 = filebuf[i++];
    flags7 = filebuf[i++];
    u8 mirror_v;
    u8 fourscreen_mir;
    // fill in flags
    mirror_v           = (flags6 >> 0) & 0x1;
    header.battery     = (flags6 >> 1) & 0x1;
    header.trainer     = (flags6 >> 2) & 0x1;
    fourscreen_mir     = (flags6 >> 3) & 0x1;
    header.mapper_num  = (flags6 >> 4) & 0xF;
    header.mapper_num |= (flags7 & 0xF0);
    INFO("Mapper Number %03d\n", header.mapper_num);

    // fill in rest of header
    header.prgram_banks = filebuf[i++];

    // set mirror mode
    // first check if four screen mode on
    if (fourscreen_mir) {
        header.mirror_mode = MIR_4SCRN;
    } else if (mirror_v) {
        header.mirror_mode = MIR_VERT;
    } else {
        header.mirror_mode = MIR_HORZ;
    }

    return header;
}

// mapper handlers
static mapper_wfunc_t map_cpuwrite = NULL;
static mapper_rfunc_t map_cpuread  = NULL;
static mapper_wfunc_t map_ppuwrite = NULL;
static mapper_rfunc_t map_ppuread  = NULL;
static mapper_init_t map_init     = NULL;

static void setup_mapper_handlers(u8 mapper_num)
{
    switch (mapper_num) {
    case 0:
        map_init     = Map000_Init;
        map_cpuwrite = Map000_CpuWrite;
        map_cpuread  = Map000_CpuRead;
        map_ppuwrite = Map000_PpuWrite;
        map_ppuread  = Map000_PpuRead;
        break;
    case 2:
        map_init     = Map002_Init;
        map_cpuwrite = Map002_CpuWrite;
        map_cpuread  = Map002_CpuRead;
        map_ppuwrite = Map002_PpuWrite;
        map_ppuread  = Map002_PpuRead;
        break;
    default:
        ERROR("Mapper (%u) not supported!\n", mapper_num);
        EXIT(1);
    }
}

static bool is_init = false;
void Cart_Init()
{
    is_init = true;
}

void Cart_Reset()
{
    if (map_init != NULL) {
        map_init(inesh.prgrom_banks, inesh.chrrom_banks);
    }
    else {
        ERROR("Cartridge Reset Failed: No Roms loaded :/\n");
        EXIT(1);
    }
}

void Cart_Load(const char *path)
{
#ifdef DEBUG
    CHECK_INIT;
#endif

    // reset memory
    if (cartmem != NULL) {
        free(cartmem);
        cartmem = NULL;
    }
    if (chrrom != NULL) {
        free(chrrom);
        chrrom = NULL;
    }

    // load new rom
    FILE *romfile = fopen(path, "rb");
    if (romfile == NULL) {
        perror("fopen");
        ERROR("Failed to open %s\n", path);
        EXIT(1);
    }

    inesh = read_ines_header(romfile);
    if (inesh.trainer) {
        // TODO add trainer support ???
        ERROR("No trainer support :(\n");
        EXIT(1);
    }

    size_t prgrom_size = inesh.prgrom_banks * PRGROM_BANK_SIZE;
    assert(prgrom_size != 0);
    chrrom_size = inesh.chrrom_banks * CHRROM_BANK_SIZE;
    if (chrrom_size == 0) {
        // TODO: Figure out CHR-RAM situation
        INFO("CHR-ROM Bank size is ZERO! Assuming CHR-RAM of 8KB\n");
        // for now, assume max size??
        chrrom_size = (8*1024);
    }
    // assert(chrrom_size != 0);

    // initialize memory
    cartmem_size = prgrom_size + (0x8000 - 0x4020);
    cartmem = malloc(cartmem_size);
    chrrom = malloc(chrrom_size);
    if (cartmem == NULL || chrrom == NULL) {
        ERROR("Out of Host Memory!\n");
        EXIT(1);
    }

    // write out prg rom
    fread(&cartmem[0x8000 - 0x4020], 1, prgrom_size, romfile);

    // may be zero if cart uses chr-ram
    fread(&chrrom[0], 1, chrrom_size, romfile);

    // init mapper handlers
    setup_mapper_handlers(inesh.mapper_num);
    map_init(inesh.prgrom_banks, inesh.chrrom_banks);

    // TODO the rare extensions

    INFO("PRG-ROM Size: %lu (%lu KB) (%u Banks)\n", prgrom_size, prgrom_size / 1024, inesh.prgrom_banks);
    INFO("CHR-ROM/RAM Size: %lu (%lu KB)\n", chrrom_size, chrrom_size / 1024);
    INFO("%s loaded successfully!\n", path);
    fclose(romfile);
}

u8 Cart_CpuRead(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
    assert(map_cpuread != NULL);
#endif
    u32 maddr = addr;
    bool allowed = map_cpuread(&maddr);
    if (allowed) {
        assert((size_t)(maddr - 0x4020) < cartmem_size);
        return cartmem[maddr - 0x4020];
    }
    return 0;
}

void Cart_CpuWrite(u8 data, u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
    assert(map_cpuwrite != NULL);
#endif
    u32 maddr = addr;
    bool allowed = map_cpuwrite(data, &maddr);
    if (allowed) {
        assert((size_t)(maddr - 0x4020) < cartmem_size);
        cartmem[maddr - 0x4020] = data;
    }
}

u8 Cart_PpuRead(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
    assert(map_ppuread != NULL);
#endif
    u32 maddr = addr;
    bool allowed = map_ppuread(&maddr);
    if (allowed) {
        assert(maddr < chrrom_size);
        return chrrom[maddr];
    }
    return 0;
}

void Cart_PpuWrite(u8 data, u16 addr)
{
#ifdef DEBUG
    CHECK_INIT;
    assert(map_ppuwrite != NULL);
#endif
    u32 maddr = addr;
    bool allowed = map_ppuwrite(data, &maddr);
    if (allowed) {
        assert(maddr < chrrom_size);
        chrrom[maddr] = data;
    }
}

inline enum mirror_mode Cart_GetMirrorMode()
{
#ifdef DEBUG
    CHECK_INIT;
#endif
    return inesh.mirror_mode;
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
    fprintf(ofile, "Mapper Num: %u\n", inesh.mapper_num);
    fprintf(ofile, "Num PRG-ROM Banks: %u\n", inesh.prgrom_banks);
    fprintf(ofile, "Num CHR-ROM Banks: %u\n", inesh.chrrom_banks);
    fprintf(ofile, "Num PRG-RAM Banks: %u\n", inesh.prgram_banks);
    fprintf(ofile, "*** Flags ***\n");
    fprintf(ofile, "    Mirror Type: %u\n", inesh.mirror_mode == MIR_VERT ? 1 : 0);
    fprintf(ofile, "    4 Screen Mirror: %u\n", inesh.mirror_mode == MIR_4SCRN ? 1 : 0);
    fprintf(ofile, "    Battery: %u\n", inesh.battery ? 1 : 0);
    fprintf(ofile, "---------------------------------------\n");
    fclose(ofile);

    // dump cartridge memory
    ofile = fopen("cartmem.dump", "wb");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump PRG-ROM\n");
        return;
    }
    fwrite(cartmem, 1, cartmem_size, ofile);
    fclose(ofile);
    ofile = NULL;

    // dump chrrom
    ofile = fopen("chr-rom.dump", "wb");
    if (ofile == NULL) {
        perror("fopen");
        ERROR("Failed to dump CHR-ROM\n");
        return;
    }
    fwrite(chrrom, 1, chrrom_size, ofile);
    fclose(ofile);
    ofile = NULL;
}
