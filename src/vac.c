/*
 * vac.c
 *
 * Travis Banken
 * 2020
 *
 * Wrapper framework for Video, Audio, and Controllers
 */

#include <string.h>

#include <vac.h>
#include <SDL3/SDL.h>

#define SDL_PERROR ERROR("SDL ERROR: %s\n", SDL_GetError())

#define RES_X 256
#define RES_Y 240
#define DBG_RES_X (128*2 + 4)

#define STICKY_LIMIT 10

static int pxscale;
static SDL_Window *window;
static SDL_Renderer *renderer;
static bool debug_on;

// video buffer
static nes_color_t vbuf[RES_X * RES_Y];
static nes_color_t pt_vbuf[2][128*128];
static nes_color_t nt_vbuf[2][RES_X*RES_Y];

// audio
static SDL_AudioStream *audio_stream;
static u8 dev_silence = 0;
static void audio_callback(void *usedata, u8 *stream, int len);

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
    if (rc != 0) {
        SDL_PERROR;
        EXIT(1);
    }
}

static u32 set_key(SDL_Keycode keycode, u32 keystate)
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
    case SDLK_RSHIFT:
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
    case SDLK_ESCAPE:
        keystate |= KEY_RESET;
        break;
    // Mute Channels
    case SDLK_1:
        keystate |= KEY_MUTE_1;
        break;
    case SDLK_2:
        keystate |= KEY_MUTE_2;
        break;
    case SDLK_3:
        keystate |= KEY_MUTE_3;
        break;
    case SDLK_4:
        keystate |= KEY_MUTE_4;
        break;
    case SDLK_5:
        keystate |= KEY_MUTE_5;
        break;
    }
    return keystate;
}

static u32 unset_key(SDL_Keycode keycode, u32 keystate)
{
    switch (keycode) {
    // Game pad keys
    case SDLK_j:
        keystate &= ~KEY_A;
        break;
    case SDLK_k:
        keystate &= ~KEY_B;
        break;
    case SDLK_w:
        keystate &= ~KEY_UP;
        break;
    case SDLK_s:
        keystate &= ~KEY_DOWN;
        break;
    case SDLK_d:
        keystate &= ~KEY_RIGHT;
        break;
    case SDLK_a:
        keystate &= ~KEY_LEFT;
        break;
    case SDLK_RETURN:
        keystate &= ~KEY_START;
        break;
    case SDLK_RSHIFT:
        keystate &= ~KEY_SELECT;
        break;
    // Debug tools
    case SDLK_n:
        keystate &= ~KEY_STEP;
        break;
    case SDLK_p:
        keystate &= ~KEY_PAUSE;
        break;
    case SDLK_c:
        keystate &= ~KEY_CONTINUE;
        break;
    case SDLK_f:
        keystate &= ~KEY_FRAME_MODE;
        break;
    case SDLK_l:
        keystate &= ~KEY_PAL_CHANGE;
        break;
    case SDLK_ESCAPE:
        keystate &= ~KEY_RESET;
        break;
    // mute channels
    case SDLK_1:
        keystate &= ~KEY_MUTE_1;
        break;
    case SDLK_2:
        keystate &= ~KEY_MUTE_2;
        break;
    case SDLK_3:
        keystate &= ~KEY_MUTE_3;
        break;
    case SDLK_4:
        keystate &= ~KEY_MUTE_4;
        break;
    case SDLK_5:
        keystate &= ~KEY_MUTE_5;
        break;
    }
    return keystate;
}

void Vac_Init(const char *title, bool debug_display)
{
    pxscale = debug_display ? 2 : 3;
    debug_on = debug_display;

    int rc;
    // init sdl
    rc = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    if (rc < 0) {
        ERROR("%s\n", SDL_GetError());
        EXIT(1);
    }

    // init audio
    // SDL_AudioSpec want, have;
    // memset(&want, 0, sizeof(want));
    // want.freq = 44100;
    // want.format = AUDIO_F32;
    // want.channels = 1;
    // want.samples = 512; // TODO: find best val (must be power of 2)
    const SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, 44100 };
    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &spec, NULL, NULL);
    if (audio_stream == NULL) {
        ERROR("Failed to create audio stream: %s\n", SDL_GetError());
        EXIT(1);
    }

    int wh = scale(RES_Y);
    int ww = scale(RES_X);
    if (debug_on) {
        ww += (scale_dbg(DBG_RES_X));
    }
    // create window
    window = SDL_CreateWindow(title, ww, wh, 0);
    if (window == NULL) {
        ERROR("%s\n", SDL_GetError());
        EXIT(1);
    }

    // create renderer
    renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        ERROR("%s\n", SDL_GetError());
        EXIT(1);
    }

    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audio_stream));
}

void Vac_Free()
{
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

u32 Vac_Poll()
{
    static u32 keystate = 0;
    SDL_Event e;
    SDL_Keycode keycode;
    if (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            EXIT(0);
            break;
        case SDL_EVENT_KEY_DOWN:
            keycode = e.key.keysym.sym;
            keystate = set_key(keycode, keystate);
            break;
        case SDL_EVENT_KEY_UP:
            keycode = e.key.keysym.sym;
            keystate = unset_key(keycode, keystate);
            break;
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
            size_t col_id = (y * RES_X) + x;
            assert(col_id < sizeof(vbuf));
            nes_color_t color = vbuf[col_id];
            int rc = SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, SDL_ALPHA_OPAQUE);
            if (rc != 0) {
                SDL_PERROR;
                EXIT(1);
            }

            SDL_FRect rectangle;
            rectangle.x = scale(x);
            rectangle.y = scale(y);
            rectangle.w = scale(1);
            rectangle.h = scale(1);
            SDL_RenderFillRect(renderer, &rectangle);
        }
    }

    // draw out debug display
    if (debug_on) {
        // draw pattern table
        for (int table_side = 0; table_side < 2; table_side++) {
            for (int y = 0; y < 128; y++) {
                for (int x = 0; x < 128; x++) {
                    // set color
                    int col_id = (y * 128) + x;
                    assert(table_side < 2);
                    assert(col_id < (128*128));
                    nes_color_t color = pt_vbuf[table_side][col_id];
                    int rc = SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, SDL_ALPHA_OPAQUE);
                    if (rc != 0) {
                        SDL_PERROR;
                        EXIT(1);
                    }

                    SDL_FRect rect;
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
    if (x >= RES_X || x < 0 || y >= RES_Y || y < 0) {
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

void Vac_SetPxNt(int table_side, u16 x, u16 y, nes_color_t color)
{
    assert(x < 256 && y < 240);
    assert(debug_on);
    assert(table_side >= 0 && table_side <= 1);

    nt_vbuf[table_side][y*256 + x] = color;
}

void Vac_ClearScreen()
{
    SDL_RenderClear(renderer);
}

unsigned int Vac_MsPassedFrom(unsigned int from)
{
    unsigned int cur_ms = SDL_GetTicks();
    return cur_ms - from;
}

unsigned int Vac_Now()
{
    return SDL_GetTicks();
}

bool Vac_OneSecPassed()
{
    static unsigned int last_ms = 0;
    unsigned int time_passed = Vac_MsPassedFrom(last_ms);
    if (time_passed >= 1000) {
        last_ms += time_passed;
        return true;
    } else {
        return false;
    }
}

void Vac_Delay(unsigned int ms)
{
    SDL_Delay(ms);
}

void Vac_SetWindowTitle(const char *title)
{
    SDL_SetWindowTitle(window, title);
}

// *********************************************************
// *** AUDIO ***
// *********************************************************

void Vac_QueueAudio(const void* data, uint32_t len) {
    int rc = SDL_PutAudioStreamData(audio_stream, data, len);
    if (rc < 0) {
        ERROR("Failed to queue audio: %s/n", SDL_GetError());
        EXIT(1);
    }
}

