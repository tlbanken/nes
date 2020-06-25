/*
 * nes.c
 *
 * Travis Banken
 * 2020
 *
 * Main code for the NES emulator.
 */

#include <signal.h>

#include <utils.h>

static void sighandler(int sig)
{
    if (sig == SIGINT) {
        exit(1);
    }
}

static void exit_handler(int rc, void *arg)
{
    // TODO
}

int main(int argc, char **argv)
{
    (void) argc, (void) argv;

    neslog_init();
    neslog_add(LID_CPU, "cpu.log");

    // TODO do the nes stuff here

    neslog_cleanup();
    return 0;
}