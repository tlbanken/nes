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
        neslog_cleanup();
    }
}

static void run()
{
    int limit = 9000;
    int rounds = 0;
    // int cycles;
    while (rounds < limit) {
        cpu_step();
        rounds++;
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
    neslog_add(LID_CPU, "cpu.log");

    // init hw
    cart_load(rompath);
    cpu_init();

    // TODO do the nes stuff here
    run();

    neslog_cleanup();
    return 0;
}

