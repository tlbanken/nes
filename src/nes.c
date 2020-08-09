/*
 * nes.c
 *
 * Travis Banken
 * 2020
 *
 * Main code for the NES emulator.
 */

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include <utils.h>
#include <mem.h>
#include <cart.h>
#include <cpu.h>
#include <ppu.h>
#include <vac.h>

static void sighandler(int sig)
{
    if (sig == SIGSEGV) {
        fprintf(stderr, "SIGSEGV (seg fault) caught!\n");
    }
    if (sig == SIGINT) {
        fprintf(stderr, "SIGINT caught!\n");
    }
    if (sig == SIGABRT) {
        fprintf(stderr, "SIGABORT caught!\n");
    }
    EXIT(1);
}

static void exit_handler(int rc)
{
    if (rc != OK) {
        Mem_Dump();
        Cart_Dump();
        Ppu_Dump();
    }
    Neslog_Free();
    Vac_Free();
}

static void run()
{
    // int limit = 9000;
    // int rounds = 0;
    int cycles;
    u32 num_frames = 0;
    // bool paused = true; // NOTE: TESTING
    bool paused = false;
    // bool frame_mode = true; // NOTE: TESTING
    bool frame_mode = false;
    bool frame_finished = false;
    u8 pal_id = 1;
    while (true) {
        u16 kc = Vac_Poll();
        if (kc & KEY_PAUSE) {
            paused = true;
        } else if (kc & KEY_CONTINUE) {
            paused = false;
            frame_mode = false;
        } else if (kc & KEY_FRAME_MODE) {
            frame_mode = !frame_mode;
            paused = true;
        }

        if (kc & KEY_PAL_CHANGE) {
            pal_id = (pal_id % 8) + 1;
        }

        // execution of cpu and ppu
        if (!paused || (kc & KEY_STEP) || (frame_mode && !frame_finished)) {
            cycles = Cpu_Step();
            frame_finished = Ppu_Step(3 * cycles);
            // rounds++;
        }

        // change pallete on debug display
        if (kc & KEY_PAL_CHANGE) {
            // draw_pattern_table(0, pal_id - 1);
            // draw_pattern_table(1, pal_id - 1);
            Vac_Refresh();
        }

        // update screen on frame finish
        if (frame_finished || (kc & KEY_STEP)) {
            // debug
            // draw_pattern_table(0, pal_id - 1);
            // draw_pattern_table(1, pal_id - 1);

            frame_finished = false;
            Vac_Refresh();
            Vac_ClearScreen();

            num_frames++;
        }

        // TODO: Only during debug mode!
        // calc frame rate
        if (Vac_OneSecPassed()) {
            // display frame rate
            printf("%d\n", num_frames);
            num_frames = 0;
        }
    }
}

int main(int argc, char **argv)
{
    (void) argc, (void) argv;

    if (argc != 2) {
        fprintf(stderr, "usage: nes <rom path>\n");
        return 1;
    }

    char *rompath = argv[1];

    int rc;

    // set up exit and signal handling
    struct sigaction sa;
    sa.sa_handler = sighandler;
    rc = sigaction(SIGINT, &sa, NULL);
    assert(rc == 0);
    rc = sigaction(SIGSEGV, &sa, NULL);
    assert(rc == 0);
    rc = sigaction(SIGABRT, &sa, NULL);
    assert(rc == 0);
    Utils_SetExitHandler(exit_handler);

    Neslog_Init();
    // Neslog_Add(LID_CPU, "cpu.log");
    // Neslog_Add(LID_CPU, NULL);
    // Neslog_Add(LID_PPU, "ppu.log");
    // Neslog_Add(LID_PPU, NULL);

    // init hw
    Mem_Init();
    Cart_Init();
    Cpu_Init();
    Ppu_Init();
    char title[64] = "NES - ";
    strncat(title, rompath, 64);
    Vac_Init(title, false);

    // load up the rom and start the game
    Cart_Load(rompath);
    Cpu_Reset();
    run();

    Vac_Free();
    Neslog_Free();
    return 0;
}

