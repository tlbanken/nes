/*
 * vac.c
 *
 * Travis Banken
 * 2020
 *
 * Wrapper framework for Video, Audio, and Controllers
 */

#include <vac.h>
#include <SDL.h>

#define RES_X 256
#define RES_Y 240
#define DBG_RES_X (128*2 + 3)

#define STICKY_LIMIT 1

static int pxscale;
static SDL_Window *window;
static SDL_Renderer *renderer;
static u16 keystate;
static u32 stickycount;
static bool debug_on;

// video buffer
static nes_color_t vbuf[RES_X * RES_Y];
static nes_color_t pt_vbuf[2][128*128];

static int scale(int val)
{
    return val * pxscale;
}

static int scale_dbg(int val)
{
    return val * 2;
}

static void reset_draw_color()
{
    int rc = SDL_SetRenderDrawColor(renderer, 0x77, 0x85, 0x8C, SDL_ALPHA_OPAQUE);
    assert(rc == 0);
}

static void sdl_to_nes_key(SDL_Keycode keycode)
{
    switch (keycode) {
    // Game pad keys
    case SDLK_j:
        keystate |= KEY_A;
        break;
    case SDLK_k:
        keystate |= KEY_B;
        break;
    case SDLK_w:
        keystate |= KEY_UP;
        break;
    case SDLK_s:
        keystate |= KEY_DOWN;
        break;
    case SDLK_d:
        keystate |= KEY_RIGHT;
        break;
    case SDLK_a:
        keystate |= KEY_LEFT;
        break;
    case SDLK_RETURN:
        keystate |= KEY_START;
        break;
    case SDLK_BACKSPACE:
        keystate |= KEY_SELECT;
        break;
    // Debug tools
    case SDLK_n:
        keystate |= KEY_STEP;
        break;
    case SDLK_p:
        keystate |= KEY_PAUSE;
        break;
    case SDLK_c:
        keystate |= KEY_CONTINUE;
        break;
    case SDLK_f:
        keystate |= KEY_FRAME_MODE;
        break;
    case SDLK_l:
        keystate |= KEY_PAL_CHANGE;
        break;
    }
}

void Vac_Init(const char *title, bool debug_display)
{
    pxscale = 4;
    stickycount = 0;
    keystate = 0;
    debug_on = debug_display;

    int rc;
    // init sdl
    rc = SDL_Init(SDL_INIT_VIDEO);
    if (rc < 0) {
        ERROR("%s\n", SDL_GetError());
        EXIT(1);
    }

    int wh = scale(RES_Y);
    int ww = scale(RES_X);
    if (debug_on) {
        ww += (scale_dbg(DBG_RES_X));
    }
    // create window
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                ww, wh, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        ERROR("%s\n", SDL_GetError());
        EXIT(1);
    }

    // create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    // renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL) {
        ERROR("%s\n", SDL_GetError());
        EXIT(1);
    }
}

void Vac_Free()
{
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

u16 Vac_Poll()
{
    // crude sticky keys impl
    if (stickycount >= STICKY_LIMIT) {
        stickycount = 0;
        keystate = 0;
    } else {
        stickycount++;
    }

    SDL_Event e;
    SDL_Keycode keycode;
    if (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            EXIT(0);
            break;
        case SDL_KEYDOWN:
            keycode = e.key.keysym.sym;
            sdl_to_nes_key(keycode);
        }
    }
    return keystate;
}

void Vac_Refresh()
{
    // draw out buffer
    for (int y = 0; y < RES_Y; y++) {
        for (int x = 0; x < RES_X; x++) {
            // set color
            nes_color_t color = vbuf[(y * RES_X) + x];
            int rc = SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, SDL_ALPHA_OPAQUE);
            assert(rc == 0);

            SDL_Rect rectangle;
            rectangle.x = scale(x);
            rectangle.y = scale(y);
            rectangle.w = scale(1);
            rectangle.h = scale(1);
            SDL_RenderFillRect(renderer, &rectangle);
        }
    }

    // draw out debug display
    if (debug_on) {
        for (int table_side = 0; table_side < 2; table_side++) {
            for (int y = 0; y < 128; y++) {
                for (int x = 0; x < 128; x++) {
                    // set color
                    nes_color_t color = pt_vbuf[table_side][y*128 + x];
                    int rc = SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, SDL_ALPHA_OPAQUE);
                    assert(rc == 0);

                    SDL_Rect rect;
                    rect.x = scale_dbg(x + 1) + scale(RES_X) + (scale_dbg(128) * table_side
                        + scale_dbg(1) * table_side);
                    rect.y = scale_dbg(y + 1);
                    rect.w = scale_dbg(1);
                    rect.h = scale_dbg(1);
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }
    }

    reset_draw_color();
    SDL_RenderPresent(renderer);
    Vac_Poll();
}

void Vac_SetPx(int x, int y, nes_color_t color)
{
    // don't draw outside screen
    if (x >= RES_X || y >= RES_Y) {
        return;
    }

    vbuf[y * RES_X + x] = color;


}

void Vac_SetPxPt(int table_side, u16 x, u16 y, nes_color_t color)
{
    assert(x < 128 && y < 128);
    assert(debug_on);
    assert(table_side >= 0 && table_side <= 1);

    pt_vbuf[table_side][y*128 + x] = color;
}

void Vac_ClearScreen()
{
    SDL_RenderClear(renderer);
}

bool Vac_OneSecPassed()
{
    static unsigned int last_ms = 0;
    unsigned int cur_ms = SDL_GetTicks();
    if (cur_ms > last_ms + 1000) {
        last_ms = cur_ms;
        return true;
    } else {
        return false;
    }
}

void Vac_SetWindowTitle(const char *title)
{
    SDL_SetWindowTitle(window, title);
}
