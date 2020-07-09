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

static enum nes_keycode sdl_to_nes_key(SDL_Keycode keycode)
{
    switch (keycode) {
    // Game pad keys
    case SDLK_j:
        return KEY_A;
    case SDLK_k:
        return KEY_B;
    case SDLK_w:
        return KEY_UP;
    case SDLK_s:
        return KEY_DOWN;
    case SDLK_d:
        return KEY_RIGHT;
    case SDLK_a:
        return KEY_LEFT;
    case SDLK_RETURN:
        return KEY_START;
    case SDLK_BACKSPACE:
        return KEY_SELECT;
    // Debug tools
    case SDLK_n:
        return KEY_STEP;
    case SDLK_p:
        return KEY_PAUSE;
    case SDLK_c:
        return KEY_CONTINUE;
    case SDLK_f:
        return KEY_FRAME_STEP;
    default:
        return KEY_NONE;
    }
}

void periphs_init(const char *title)
{
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

enum nes_keycode periphs_poll()
{
    SDL_Event e;
    SDL_Keycode keycode;
    if (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            EXIT(0);
            break;
        case SDL_KEYDOWN:
            keycode = e.key.keysym.sym;
            return sdl_to_nes_key(keycode);
        }
    }
    return KEY_NONE;
}

void periphs_refresh()
{
    SDL_RenderPresent(renderer);
    periphs_poll();
}

void set_px(int x, int y, nes_color_t color)
{
    // don't draw outside screen
    if (x >= RES_X || y >= RES_Y) {
        return;
    }

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
