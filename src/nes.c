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

static void sighandler(int sig)
{
    if (sig == SIGINT) {
        EXIT(1);
    }
}

static void exit_handler(int rc)
{
    if (rc != OK) {
        mem_dump();
        neslog_cleanup();
    }
}

int main(int argc, char **argv)
{
    (void) argc, (void) argv;

    int rc;

    // set up exit and signal handling
    struct sigaction sa;
    sa.sa_handler = sighandler;
    rc = sigaction(SIGINT, &sa, NULL);
    assert(rc == 0);
    set_exit_handler(exit_handler);

    neslog_init();
    neslog_add(LID_CPU, "cpu.log");

    // TODO do the nes stuff here

    neslog_cleanup();
    return 0;
}