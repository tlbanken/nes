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
        ERROR("Invalid log id (%d)\n", id);
        exit(1);
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
    /*03 -> */"undef",
    /*04 -> */"undef",
    /*05 -> */"ora",
    /*06 -> */"asl",
    /*07 -> */"undef",
    /*08 -> */"php",
    /*09 -> */"ora",
    /*0A -> */"asl",
    /*0B -> */"undef",
    /*0C -> */"undef",
    /*0D -> */"ora",
    /*0E -> */"asl",
    /*0F -> */"undef",
    // MSD 1
    /*10 -> */"bpl",
    /*11 -> */"ora",
    /*12 -> */"undef",
    /*13 -> */"undef",
    /*14 -> */"undef",
    /*15 -> */"ora",
    /*16 -> */"asl",
    /*17 -> */"undef",
    /*18 -> */"clc",
    /*19 -> */"ora",
    /*1A -> */"undef",
    /*1B -> */"undef",
    /*1C -> */"undef",
    /*1D -> */"ora",
    /*1E -> */"asl",
    /*1F -> */"undef",
    // MSD 2
    /*20 -> */"jsr",
    /*21 -> */"and",
    /*22 -> */"undef",
    /*23 -> */"undef",
    /*24 -> */"bit",
    /*25 -> */"and",
    /*26 -> */"rol",
    /*27 -> */"undef",
    /*28 -> */"plp",
    /*29 -> */"and",
    /*2A -> */"rol",
    /*2B -> */"undef",
    /*2C -> */"bit",
    /*2D -> */"and",
    /*2E -> */"rol",
    /*2F -> */"undef",
    // MSD 3
    /*30 -> */"bmi",
    /*31 -> */"and",
    /*32 -> */"undef",
    /*33 -> */"undef",
    /*34 -> */"undef",
    /*35 -> */"and",
    /*36 -> */"rol",
    /*37 -> */"undef",
    /*38 -> */"sec",
    /*39 -> */"and",
    /*3A -> */"undef",
    /*3B -> */"undef",
    /*3C -> */"undef",
    /*3D -> */"and",
    /*3E -> */"rol",
    /*3F -> */"undef",
    // MSD 4
    /*40 -> */"rti",
    /*41 -> */"eor",
    /*42 -> */"undef",
    /*43 -> */"undef",
    /*44 -> */"undef",
    /*45 -> */"eor",
    /*46 -> */"lsr",
    /*47 -> */"undef",
    /*48 -> */"pha",
    /*49 -> */"eor",
    /*4A -> */"lsr",
    /*4B -> */"undef",
    /*4C -> */"jmp",
    /*4D -> */"eor",
    /*4E -> */"lsr",
    /*4F -> */"undef",
    // MSD 5
    /*50 -> */"bvc",
    /*51 -> */"eor",
    /*52 -> */"undef",
    /*53 -> */"undef",
    /*54 -> */"undef",
    /*55 -> */"eor",
    /*56 -> */"lsr",
    /*57 -> */"undef",
    /*58 -> */"cli",
    /*59 -> */"eor",
    /*5A -> */"undef",
    /*5B -> */"undef",
    /*5C -> */"undef",
    /*5D -> */"eor",
    /*5E -> */"lsr",
    /*5F -> */"undef",
    // MSD 6
    /*60 -> */"rts",
    /*61 -> */"adc",
    /*62 -> */"undef",
    /*63 -> */"undef",
    /*64 -> */"undef",
    /*65 -> */"adc",
    /*66 -> */"ror",
    /*67 -> */"undef",
    /*68 -> */"pla",
    /*69 -> */"adc",
    /*6A -> */"ror",
    /*6B -> */"undef",
    /*6C -> */"jmp",
    /*6D -> */"adc",
    /*6E -> */"ror",
    /*6F -> */"undef",
    // MSD 7
    /*70 -> */"bvs",
    /*71 -> */"adc",
    /*72 -> */"undef",
    /*73 -> */"undef",
    /*74 -> */"undef",
    /*75 -> */"adc",
    /*76 -> */"ror",
    /*77 -> */"undef",
    /*78 -> */"sei",
    /*79 -> */"adc",
    /*7A -> */"undef",
    /*7B -> */"undef",
    /*7C -> */"undef",
    /*7D -> */"adc",
    /*7E -> */"ror",
    /*7F -> */"undef",
    // MSD 8
    /*80 -> */"undef",
    /*81 -> */"sta",
    /*82 -> */"undef",
    /*83 -> */"undef",
    /*84 -> */"sty",
    /*85 -> */"sta",
    /*86 -> */"stx",
    /*87 -> */"undef",
    /*88 -> */"dey",
    /*89 -> */"undef",
    /*8A -> */"txa",
    /*8B -> */"undef",
    /*8C -> */"sty",
    /*8D -> */"sta",
    /*8E -> */"stx",
    /*8F -> */"undef",
    // MSD 9
    /*90 -> */"bcc",
    /*91 -> */"sta",
    /*92 -> */"undef",
    /*93 -> */"undef",
    /*94 -> */"sty",
    /*95 -> */"sta",
    /*96 -> */"stx",
    /*97 -> */"undef",
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
    /*A3 -> */"undef",
    /*A4 -> */"ldy",
    /*A5 -> */"lda",
    /*A6 -> */"ldx",
    /*A7 -> */"undef",
    /*A8 -> */"tay",
    /*A9 -> */"lda",
    /*AA -> */"tax",
    /*AB -> */"undef",
    /*AC -> */"ldy",
    /*AD -> */"lda",
    /*AE -> */"ldx",
    /*AF -> */"undef",
    // MSD 11
    /*B0 -> */"bcs",
    /*B1 -> */"lda",
    /*B2 -> */"undef",
    /*B3 -> */"undef",
    /*B4 -> */"ldy",
    /*B5 -> */"lda",
    /*B6 -> */"ldx",
    /*B7 -> */"undef",
    /*B8 -> */"clv",
    /*B9 -> */"lda",
    /*BA -> */"tsx",
    /*BB -> */"undef",
    /*BC -> */"ldy",
    /*BD -> */"lda",
    /*BE -> */"ldx",
    /*BF -> */"undef",
    // MSD 12
    /*C0 -> */"cpy",
    /*C1 -> */"cmp",
    /*C2 -> */"undef",
    /*C3 -> */"undef",
    /*C4 -> */"cpy",
    /*C5 -> */"cmp",
    /*C6 -> */"dec",
    /*C7 -> */"undef",
    /*C8 -> */"iny",
    /*C9 -> */"cmp",
    /*CA -> */"dex",
    /*CB -> */"undef",
    /*CC -> */"cpy",
    /*CD -> */"cmp",
    /*CE -> */"dec",
    /*CF -> */"undef",
    // MSD 13
    /*D0 -> */"bne",
    /*D1 -> */"cmp",
    /*D2 -> */"undef",
    /*D3 -> */"undef",
    /*D4 -> */"undef",
    /*D5 -> */"cmp",
    /*D6 -> */"dec",
    /*D7 -> */"undef",
    /*D8 -> */"cld",
    /*D9 -> */"cmp",
    /*DA -> */"undef",
    /*DB -> */"undef",
    /*DC -> */"undef",
    /*DD -> */"cmp",
    /*DE -> */"dec",
    /*DF -> */"undef",
    // MSD 14
    /*E0 -> */"cpx",
    /*E1 -> */"sbc",
    /*E2 -> */"undef",
    /*E3 -> */"undef",
    /*E4 -> */"cpx",
    /*E5 -> */"sbc",
    /*E6 -> */"inc",
    /*E7 -> */"undef",
    /*E8 -> */"inx",
    /*E9 -> */"sbc",
    /*EA -> */"nop",
    /*EB -> */"undef",
    /*EC -> */"cpx",
    /*ED -> */"sbc",
    /*EE -> */"inc",
    /*EF -> */"undef",
    // MSD 15
    /*F0 -> */"beq",
    /*F1 -> */"sbc",
    /*F2 -> */"undef",
    /*F3 -> */"undef",
    /*F4 -> */"undef",
    /*F5 -> */"sbc",
    /*F6 -> */"inc",
    /*F7 -> */"undef",
    /*F8 -> */"sed",
    /*F9 -> */"sbc",
    /*FA -> */"undef",
    /*FB -> */"undef",
    /*FC -> */"undef",
    /*FD -> */"sbc",
    /*FE -> */"inc",
    /*FF -> */"undef"
};

char* op_to_str(u8 opcode)
{
    static char *opstring = NULL;
    int op_index = ((opcode >> 4) & 0xF) * 16 + (opcode & 0xF); 
    opstring = op_str[op_index];
    return opstring;
}

