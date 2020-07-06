/*
 * utils.c
 *
 * Travis Banken
 * 2020
 *
 * Utilities for the nes emulator.
 */

#include <stdlib.h>
#include <utils.h>

#define LMAP_SIZE 4
static FILE* lmap[LMAP_SIZE] = {0};

static bool log_on = false;
static void (*ehandler)(int) = NULL;

void neslog_init()
{
    log_on = true;
}

void neslog_cleanup()
{
    for (int id = 0; id < LMAP_SIZE; id++) {
        if (lmap[id] != NULL && lmap[id] != stdout && lmap[id] != stderr) {
            fclose(lmap[id]);
            lmap[id] = NULL;
        }
    }
}

void neslog_add(lid_t id, char *path)
{
    // default to stderr
    if (path == NULL) {
        lmap[id] = stderr;
        return;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror("fopen");
        EXIT(1);
    }
    lmap[id] = file;
}

void neslog(lid_t id, const char *fmt, ...)
{
    if (!log_on) {
        return;
    }

    // make sure log id valid
    if (lmap[id] == NULL) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(lmap[id], fmt, args);
    va_end(args);
}

void set_exit_handler(void (*func)(int))
{
    ehandler = func;
}

void exit_with_handler(int rc)
{
    if (ehandler != NULL) {
        ehandler(rc);
    }
    exit(rc);
}

static char *op_str[] =
{
    // MSD 0
    /*00 -> */"brk",
    /*01 -> */"ora",
    /*02 -> */"undef",
    /*03 -> */"*slo",
    /*04 -> */"*nop",
    /*05 -> */"ora",
    /*06 -> */"asl",
    /*07 -> */"*slo",
    /*08 -> */"php",
    /*09 -> */"ora",
    /*0A -> */"asl",
    /*0B -> */"undef",
    /*0C -> */"*nop",
    /*0D -> */"ora",
    /*0E -> */"asl",
    /*0F -> */"*slo",
    // MSD 1
    /*10 -> */"bpl",
    /*11 -> */"ora",
    /*12 -> */"undef",
    /*13 -> */"*slo",
    /*14 -> */"*nop",
    /*15 -> */"ora",
    /*16 -> */"asl",
    /*17 -> */"*slo",
    /*18 -> */"clc",
    /*19 -> */"ora",
    /*1A -> */"*nop",
    /*1B -> */"*slo",
    /*1C -> */"*nop",
    /*1D -> */"ora",
    /*1E -> */"asl",
    /*1F -> */"*slo",
    // MSD 2
    /*20 -> */"jsr",
    /*21 -> */"and",
    /*22 -> */"undef",
    /*23 -> */"*rla",
    /*24 -> */"bit",
    /*25 -> */"and",
    /*26 -> */"rol",
    /*27 -> */"*rla",
    /*28 -> */"plp",
    /*29 -> */"and",
    /*2A -> */"rol",
    /*2B -> */"undef",
    /*2C -> */"bit",
    /*2D -> */"and",
    /*2E -> */"rol",
    /*2F -> */"*rla",
    // MSD 3
    /*30 -> */"bmi",
    /*31 -> */"and",
    /*32 -> */"undef",
    /*33 -> */"*rla",
    /*34 -> */"*nop",
    /*35 -> */"and",
    /*36 -> */"rol",
    /*37 -> */"*rla",
    /*38 -> */"sec",
    /*39 -> */"and",
    /*3A -> */"*nop",
    /*3B -> */"*rla",
    /*3C -> */"*nop",
    /*3D -> */"and",
    /*3E -> */"rol",
    /*3F -> */"*rla",
    // MSD 4
    /*40 -> */"rti",
    /*41 -> */"eor",
    /*42 -> */"undef",
    /*43 -> */"*sre",
    /*44 -> */"*nop",
    /*45 -> */"eor",
    /*46 -> */"lsr",
    /*47 -> */"*sre",
    /*48 -> */"pha",
    /*49 -> */"eor",
    /*4A -> */"lsr",
    /*4B -> */"undef",
    /*4C -> */"jmp",
    /*4D -> */"eor",
    /*4E -> */"lsr",
    /*4F -> */"*sre",
    // MSD 5
    /*50 -> */"bvc",
    /*51 -> */"eor",
    /*52 -> */"undef",
    /*53 -> */"*sre",
    /*54 -> */"*nop",
    /*55 -> */"eor",
    /*56 -> */"lsr",
    /*57 -> */"*sre",
    /*58 -> */"cli",
    /*59 -> */"eor",
    /*5A -> */"*nop",
    /*5B -> */"*sre",
    /*5C -> */"*nop",
    /*5D -> */"eor",
    /*5E -> */"lsr",
    /*5F -> */"*sre",
    // MSD 6
    /*60 -> */"rts",
    /*61 -> */"adc",
    /*62 -> */"undef",
    /*63 -> */"*rra",
    /*64 -> */"*nop",
    /*65 -> */"adc",
    /*66 -> */"ror",
    /*67 -> */"*rra",
    /*68 -> */"pla",
    /*69 -> */"adc",
    /*6A -> */"ror",
    /*6B -> */"undef",
    /*6C -> */"jmp",
    /*6D -> */"adc",
    /*6E -> */"ror",
    /*6F -> */"*rra",
    // MSD 7
    /*70 -> */"bvs",
    /*71 -> */"adc",
    /*72 -> */"undef",
    /*73 -> */"*rra",
    /*74 -> */"*nop",
    /*75 -> */"adc",
    /*76 -> */"ror",
    /*77 -> */"*rra",
    /*78 -> */"sei",
    /*79 -> */"adc",
    /*7A -> */"*nop",
    /*7B -> */"*rra",
    /*7C -> */"*nop",
    /*7D -> */"adc",
    /*7E -> */"ror",
    /*7F -> */"*rra",
    // MSD 8
    /*80 -> */"*nop",
    /*81 -> */"sta",
    /*82 -> */"*nop",
    /*83 -> */"*sax",
    /*84 -> */"sty",
    /*85 -> */"sta",
    /*86 -> */"stx",
    /*87 -> */"*sax",
    /*88 -> */"dey",
    /*89 -> */"*nop",
    /*8A -> */"txa",
    /*8B -> */"undef",
    /*8C -> */"sty",
    /*8D -> */"sta",
    /*8E -> */"stx",
    /*8F -> */"*sax",
    // MSD 9
    /*90 -> */"bcc",
    /*91 -> */"sta",
    /*92 -> */"undef",
    /*93 -> */"undef",
    /*94 -> */"sty",
    /*95 -> */"sta",
    /*96 -> */"stx",
    /*97 -> */"*sax",
    /*98 -> */"tya",
    /*99 -> */"sta",
    /*9A -> */"txs",
    /*9B -> */"undef",
    /*9C -> */"undef",
    /*9D -> */"sta",
    /*9E -> */"undef",
    /*9F -> */"undef",
    // MSD 10
    /*A0 -> */"ldy",
    /*A1 -> */"lda",
    /*A2 -> */"ldx",
    /*A3 -> */"*lax",
    /*A4 -> */"ldy",
    /*A5 -> */"lda",
    /*A6 -> */"ldx",
    /*A7 -> */"*lax",
    /*A8 -> */"tay",
    /*A9 -> */"lda",
    /*AA -> */"tax",
    /*AB -> */"undef",
    /*AC -> */"ldy",
    /*AD -> */"lda",
    /*AE -> */"ldx",
    /*AF -> */"*lax",
    // MSD 11
    /*B0 -> */"bcs",
    /*B1 -> */"lda",
    /*B2 -> */"undef",
    /*B3 -> */"*lax",
    /*B4 -> */"ldy",
    /*B5 -> */"lda",
    /*B6 -> */"ldx",
    /*B7 -> */"*lax",
    /*B8 -> */"clv",
    /*B9 -> */"lda",
    /*BA -> */"tsx",
    /*BB -> */"undef",
    /*BC -> */"ldy",
    /*BD -> */"lda",
    /*BE -> */"ldx",
    /*BF -> */"*lax",
    // MSD 12
    /*C0 -> */"cpy",
    /*C1 -> */"cmp",
    /*C2 -> */"*nop",
    /*C3 -> */"*dcp",
    /*C4 -> */"cpy",
    /*C5 -> */"cmp",
    /*C6 -> */"dec",
    /*C7 -> */"*dcp",
    /*C8 -> */"iny",
    /*C9 -> */"cmp",
    /*CA -> */"dex",
    /*CB -> */"undef",
    /*CC -> */"cpy",
    /*CD -> */"cmp",
    /*CE -> */"dec",
    /*CF -> */"*dcp",
    // MSD 13
    /*D0 -> */"bne",
    /*D1 -> */"cmp",
    /*D2 -> */"undef",
    /*D3 -> */"*dcp",
    /*D4 -> */"*nop",
    /*D5 -> */"cmp",
    /*D6 -> */"dec",
    /*D7 -> */"*dcp",
    /*D8 -> */"cld",
    /*D9 -> */"cmp",
    /*DA -> */"*nop",
    /*DB -> */"*dcp",
    /*DC -> */"*nop",
    /*DD -> */"cmp",
    /*DE -> */"dec",
    /*DF -> */"*dcp",
    // MSD 14
    /*E0 -> */"cpx",
    /*E1 -> */"sbc",
    /*E2 -> */"*nop",
    /*E3 -> */"*isc",
    /*E4 -> */"cpx",
    /*E5 -> */"sbc",
    /*E6 -> */"inc",
    /*E7 -> */"*isc",
    /*E8 -> */"inx",
    /*E9 -> */"sbc",
    /*EA -> */"nop",
    /*EB -> */"*sbc",
    /*EC -> */"cpx",
    /*ED -> */"sbc",
    /*EE -> */"inc",
    /*EF -> */"*isc",
    // MSD 15
    /*F0 -> */"beq",
    /*F1 -> */"sbc",
    /*F2 -> */"undef",
    /*F3 -> */"*isc",
    /*F4 -> */"*nop",
    /*F5 -> */"sbc",
    /*F6 -> */"inc",
    /*F7 -> */"*isc",
    /*F8 -> */"sed",
    /*F9 -> */"sbc",
    /*FA -> */"*nop",
    /*FB -> */"*isc",
    /*FC -> */"*nop",
    /*FD -> */"sbc",
    /*FE -> */"inc",
    /*FF -> */"*isc"
};

char* op_to_str(u8 opcode)
{
    static char *opstring = NULL;
    int op_index = ((opcode >> 4) & 0xF) * 16 + (opcode & 0xF); 
    opstring = op_str[op_index];
    return opstring;
}

