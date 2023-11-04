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
#include <apu.h>
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

static void run(const char *title, bool dbg_mode)
{
    char title_fps[128];
    char fps[64];

    unsigned int last_frame_ms = Vac_Now();

    // int limit = 9000;
    // int rounds = 0;
    u32 cycles = 0;
    u32 num_frames = 0;
    u32 cpf = 0;
    u32 mcpf = 0;
    // bool paused = true; // NOTE: TESTING
    bool paused = false;
    // bool frame_mode = true; // NOTE: TESTING
    bool frame_mode = false;
    bool frame_finished = false;
    u8 pal_id = 1;
    while (true) {
        // poll keyboard
        u32 kc = Vac_Poll();
        if (kc & KEY_PAUSE) {
            paused = true;
        } else if (kc & KEY_CONTINUE) {
            paused = false;
            frame_mode = false;
        } else if (kc & KEY_FRAME_MODE) {
            frame_mode = !frame_mode;
            paused = true;
        } else if (kc & KEY_RESET) {
            return;
        }

        // update palette for debug display
        if ((kc & KEY_PAL_CHANGE) && dbg_mode) {
            static unsigned int last_pal_update = 0;
            unsigned int passed = 0;
            if ((passed = Vac_MsPassedFrom(last_pal_update)) >= 200) {
                pal_id = (pal_id % 8) + 1;
                last_pal_update += passed;
            }
        }

        // execution of cpu, ppu, and apu
        if (!paused || (kc & KEY_STEP) || (frame_mode && !frame_finished)) {
            // if we aren't in step mode, we advance cpu n many cycles
            if (kc & KEY_STEP) {
                cycles = Cpu_Step();
            } else {
                while (cycles < 10) {
                    cycles += Cpu_Step();
                }
            }
            frame_finished = Ppu_Step(3 * cycles);
            Apu_Step(cycles / 2, kc);
            cpf += cycles;
            cycles = 0;
        }

        // change pallete on debug display
        if (kc & KEY_PAL_CHANGE && dbg_mode && paused) {
            Ppu_DrawPT(0, pal_id - 1);
            Ppu_DrawPT(1, pal_id - 1);
            Vac_Refresh();
        }

        // update screen on frame finish
        if (frame_finished || (kc & KEY_STEP)) {
            // debug
            if (dbg_mode) {
                Ppu_DrawPT(0, pal_id - 1);
                Ppu_DrawPT(1, pal_id - 1);
            }

            frame_finished = false;
            Vac_Refresh();
            Vac_ClearScreen();

            // one frame should take about 17 ms
            unsigned int passed;
            if ((passed = Vac_MsPassedFrom(last_frame_ms)) < 16) {
                Vac_Delay(16 - passed);
            }
            last_frame_ms = Vac_Now();
            num_frames++;
            mcpf = cpf > mcpf ? cpf : mcpf;
            cpf = 0;
        }

        // TODO: Only during debug mode!
        // calc frame rate
        if (Vac_OneSecPassed()) {
            // display frame rate
            strncpy(title_fps, title, 64);
            sprintf(fps, " | %d fps | CPU: %0.3lf MHz", num_frames, 
                (double) (mcpf * num_frames) / 1000000.0);
            strncat(title_fps, fps, 64);
            Vac_SetWindowTitle(title_fps);
            num_frames = 0;
        } 
        // else if (num_frames > 60) {
        //     strncpy(title_fps, title, 64);
        //     strncat(title_fps, " - 60 fps (capped)", 64);
        //     Vac_SetWindowTitle(title_fps);
        //     // cap to 60 fps
        //     // while(!Vac_OneSecPassed()) {
        //     //     Vac_Poll();
        //     //     // SDL_Delay(20); // Take some load off of cpu (may cause lag)
        //     // }
        //     num_frames = 0;
        // }
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
    Apu_Init();
    char title[64] = "NES - ";
    strncat(title, rompath, 64);
    bool dbg_mode = false;
    Vac_Init(title, dbg_mode);

    // run only returns on NES RESET
    while (1) {
        // NOTE: cartridge must be loaded before any other reset
        Cart_Load(rompath);

        Cpu_Reset();
        Ppu_Reset();
        Apu_Reset();
        run(title, dbg_mode);
        Vac_ClearScreen();
    }

    Vac_Free();
    Neslog_Free();
    return 0;
}

