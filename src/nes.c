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
#include <periphs.h>

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
        mem_dump();
        cart_dump();
        ppu_dump();
        neslog_cleanup();
        periphs_free();
    }
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
        u16 kc = periphs_poll();
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

        if (!paused || (kc & KEY_STEP) || (frame_mode && !frame_finished)) {
            cycles = cpu_step();
            frame_finished = ppu_step(3 * cycles);
            // rounds++;
            // if (rounds % 300 == 0 || (kc & KEY_STEP)) {
            //     draw_pattern_table(0, pal_id - 1);
            //     draw_pattern_table(1, pal_id - 1);
            //     periphs_refresh();
            // }
        }

        if (kc & KEY_PAL_CHANGE) {
            draw_pattern_table(0, pal_id - 1);
            draw_pattern_table(1, pal_id - 1);
            periphs_refresh();
        }

        if (frame_finished || (kc & KEY_STEP)) {
            // debug
            draw_pattern_table(0, pal_id - 1);
            draw_pattern_table(1, pal_id - 1);

            frame_finished = false;
            periphs_refresh();
            clear_screen();

            num_frames++;
        }

        // TODO: Only during debug mode!
        // calc frame rate
        if (periphs_one_sec_passed()) {
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
    set_exit_handler(exit_handler);

    neslog_init();
    // neslog_add(LID_CPU, "cpu.log");
    // neslog_add(LID_CPU, NULL);
    // neslog_add(LID_PPU, "ppu.log");
    // neslog_add(LID_PPU, NULL);

    // init hw
    mem_init();
    cart_init(rompath);
    cpu_init();
    ppu_init();
    char title[64] = "NES - ";
    strncat(title, rompath, 64);
    periphs_init(title, true);

    // TODO do the nes stuff here
    run();

    periphs_free();
    neslog_cleanup();
    return 0;
}

