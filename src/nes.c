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

#include <utils.h>
#include <mem.h>
#include <cart.h>
#include <cpu.h>
#include <ppu.h>
#include <periphs.h>

static void sighandler(int sig)
{
    if (sig == SIGSEGV) {
        fprintf(stderr, "SEG FAULT!\n");
    }
    if (sig == SIGINT) {
        fprintf(stderr, "SIGINT caught!\n");
    }
    if (sig == SIGABRT) {
        fprintf(stderr, "SIGABORT (caught)!\n");
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
    int rounds = 0;
    int cycles;
    bool paused = false;
    bool frame_mode = false;
    bool frame_finished = false;
    while (true) {
        enum nes_keycode kc = periphs_poll();
        if (kc == KEY_PAUSE) {
            paused = true;
        } else if (kc == KEY_CONTINUE) {
            paused = false;
            frame_mode = false;
        } else if (kc == KEY_FRAME_STEP) {
            frame_mode = true;
            paused = true;
        }

        if (!paused || kc == KEY_STEP || (frame_mode && !frame_finished)) {
            cycles = cpu_step();
            frame_finished = ppu_step(3 * cycles);
            rounds++;
            if (rounds % 50 == 0 || kc == KEY_STEP) {
                periphs_refresh();
            }
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
    // neslog_add(LID_PPU, "ppu.log");

    // init hw
    cart_load(rompath);
    cpu_init();
    ppu_init();
    periphs_init();
    mem_set_mirror_mode(cart_get_mirror_mode());

    // TODO do the nes stuff here
    run();

    periphs_free();
    neslog_cleanup();
    return 0;
}

