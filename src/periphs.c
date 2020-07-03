/*
 * periphs.c
 *
 * Travis Banken
 * 2020
 *
 * Wrapper around frameworks for peripherals and gui.
 */

#include <periphs.h>
#include <SDL.h>

#define RES_X 256
#define RES_Y 240

static int pxscale;
static SDL_Window *window;
static SDL_Renderer *renderer;

static int scale(int val)
{
    return val * pxscale;
}

void periphs_init()
{
    char title[] = "WORK IN PROGRESS";
    pxscale = 4;

    int rc;
    // init sdl
    rc = SDL_Init(SDL_INIT_VIDEO);
    if (rc < 0) {
        ERROR("%s\n", SDL_GetError());
        EXIT(1);
    }

    int wh = scale(RES_Y);
    int ww = scale(RES_X);
    // create window
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                ww, wh, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        ERROR("%s\n", SDL_GetError());
        EXIT(1);
    }

    // create renderer
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL) {
        ERROR("%s\n", SDL_GetError());
        EXIT(1);
    }
}

void periphs_free()
{
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

void periphs_poll()
{
    SDL_Event e;
    if (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            EXIT(0);
        }
    }
}

void periphs_refresh()
{
    SDL_RenderPresent(renderer);
    periphs_poll();
}

void set_px(int x, int y, nes_color_t color)
{
    // TODO update frame buffer

    // TODO set color
    int rc = SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, SDL_ALPHA_OPAQUE);
    assert(rc == 0);

    SDL_Rect rectangle;
    rectangle.x = scale(x);
    rectangle.y = scale(y);
    rectangle.w = scale(1);
    rectangle.h = scale(1);
    SDL_RenderFillRect(renderer, &rectangle);
}

void clear_screen()
{
    SDL_RenderClear(renderer);
}