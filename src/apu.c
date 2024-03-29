/*
 * apu.c
 *
 * Travis Banken
 * 2020
 *
 * Audio Processing Unit for the NES.
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <utils.h>
#include <apu.h>
#include <vac.h>
#include <cpu.h>

#define CHECK_INIT if(!is_init){ERROR("Not Initialized!\n"); EXIT(1);}

static u8 len_table[] = {
    /*00*/  10, /*01*/ 254, /*02*/  20, /*03*/   2, /*04*/  40, /*05*/   4,
    /*06*/  80, /*07*/   6, /*08*/ 160, /*09*/   8, /*0A*/  60, /*0B*/  10,
    /*0C*/  14, /*0D*/  12, /*0E*/  26, /*0F*/  14, /*10*/  12, /*11*/  16,
    /*12*/  24, /*13*/  18, /*14*/  48, /*15*/  20, /*16*/  96, /*17*/  22,
    /*18*/ 192, /*19*/  24, /*1A*/  72, /*1B*/  26, /*1C*/  16, /*1D*/  28,
    /*1E*/  32, /*1F*/  30
};

// Channel Enable Flags
enum APU_FLAGS {
    FLAGS_PULSE1    = 1 << 0,
    FLAGS_PULSE2    = 1 << 1,
    FLAGS_TRIANGLE  = 1 << 2,
    FLAGS_NOISE     = 1 << 3,
    FLAGS_DMC       = 1 << 4,
    FLAGS_FRAME_INT = 1 << 6,
    FLAGS_DMC_INT   = 1 << 7,
};
static u8 apuflags = 0;

// Frame Counter flags
#define COUNTER_4STEP 0
#define COUNTER_5STEP 1
static u8 counter_mode = 0;
static bool irq_disabled = false;


// structure of a pulse wave channel
typedef struct pulse_channel {
    bool enabled;
    bool halt_counter;
    bool const_vol;
    bool mute;
    float duty;
    u16 timer;
    u16 counter;
    u8 volume;
    int t_phase;
    struct {
        u8 on: 1;
        u8 period: 3;
        u8 negate: 1;
        u8 shift: 3;
    } sweep;
    // helper
    int warm_up;
} pulse_channel_t;
static pulse_channel_t pulse[2] = {0};

typedef struct triangle_channel {
    bool enabled;
    bool halt_counter;
    bool reload;
    bool mute;
    u8 lin_counter;
    u8 lin_counter_reload;
    u16 timer;
    u16 counter;
    int t_phase;
    // helper
    int warm_up;
    int warm_up_step;
} triangle_channel_t;
static triangle_channel_t triangle;

typedef struct noise_channel {
    bool enabled;
    bool mute;
    bool halt_counter;
    bool const_vol;
    u8 counter;
    u8 volume;
    u8 mode;
    u16 shift_reg;

} noise_channel_t;
static noise_channel_t noise;

static bool is_init = false;

// the higher the number, the better the approximation to square wave
#define SQR_ITER 20
#define TRI_ITER 20
#define MASTER_VOLUME 0.1f
#define CPU_CLOCK_RATE 1789773
#define PI 3.14159265f

// audio buffer
#define AUDIO_BUFFER_SIZE 4096 // Keep in mind the num samples def in vac.c
static float audio_buf[AUDIO_BUFFER_SIZE];

// fast sine approx as described here:
// https://www.youtube.com/watch?v=1xlCVBIF_ig
static float fast_sin(float x)
{
    float t = x * (1.0f / (2.0f * PI));
    t = t - (int) t;
    if (t < 0.5f) {
        return -16.0f * (t*t) + 8.0f*t;
    } else {
        return 16.0f * (t*t) - 24.0f*t + 8.0f;
    }
}

static float gen_pulse_sample(int channel)
{
    if (pulse[channel].timer < 8 || !pulse[channel].enabled || pulse[channel].mute) {
        return 0.0f;
    }

    float tau = (float) pulse[channel].t_phase++ / 44100.0f;

    // freq calc based on https://wiki.nesdev.com/w/index.php/APU
    float note = CPU_CLOCK_RATE / (16 * (pulse[channel].timer));
    float duty = pulse[channel].duty;

    float res1 = 0.0f;
    float res2 = 0.0f;
    for (int i = 1; i <= SQR_ITER; i++) {
        res1 += (fast_sin(note * 2.0f * PI * (float)i * tau) / (float)i);
        res2 += (fast_sin((note * tau - duty) * 2.0f * PI * (float)i) / (float)i);
    }

    float res = res1 - res2;
    float volume = 1.0f;
    if (pulse[channel].const_vol) {
        volume = (float) pulse[channel].volume / 15.0f;
    } 
    // warm_up_cap will remove the harsh clicks/pops at the start and end of note.
    // The number is arbitrary, too low and the pops remain, but too high and the attack is too soft.
    const int warm_up_cap = 250;
    volume *= (pulse[channel].warm_up / (float)warm_up_cap);
    pulse[channel].warm_up += 1;
    if (pulse[channel].warm_up > warm_up_cap) pulse[channel].warm_up = warm_up_cap;
    return volume * MASTER_VOLUME * res;
}

static float gen_triangle_sample()
{
    if (!triangle.enabled || triangle.mute) {
        return 0.0f;
    }

    float note = CPU_CLOCK_RATE / (32.0f * ((float) triangle.timer + 1.0f));
    float tau = (float) triangle.t_phase++ / 44100.0f;

    float res = 0.0f;
    for (int i = 0; i < TRI_ITER; i++) {
        int sign = i % 2 ? -1 : 1;
        int n = (i << 1) + 1;
        res += (float) sign 
            * (1.0f / (float) (n * n))
            * fast_sin(2.0f * PI * note * (float) n * tau);
    }
    res *= (8.0f / (PI * PI));
    // TODO: how to deal with pop on release? Need decay or something...
    // warm_up_cap will remove the harsh clicks/pops. 
    // The number is arbitrary, too low and the pops remain, but too high and the attack is too soft.
    // const int warm_up_cap = 250;
    // res *= (triangle.warm_up / (float)warm_up_cap);
    // triangle.warm_up += triangle.warm_up_step;
    // ERROR("WARMUP: %d\n", triangle.warm_up);
    // if (triangle.warm_up > warm_up_cap) triangle.warm_up = warm_up_cap;
    // if (triangle.warm_up < 0) {
    //     triangle.enabled = false;
    // }
    return MASTER_VOLUME * res;
}

static float gen_noise_sample()
{
    if (!noise.enabled || noise.mute) {
        return 0.0f;
    }
    int bit = noise.mode ? 6 : 1;
    // gen next random sequence
    // printf("SHIFT %u\n", noise.shift_reg);
    u16 new = ((noise.shift_reg >> bit) & 0x1) ^ (noise.shift_reg & 0x1);
    noise.shift_reg >>= 1;
    noise.shift_reg |= (new << 14);
    // printf("SHIFT %u\n", noise.shift_reg);
    // EXIT(0);

    srand(noise.shift_reg);
    float res = (float) (rand() % 256) / 256.0f;
    // printf("NOISE %f\n", res);
    return res;
}

void Apu_Init()
{
    is_init = true;

    // setup audio callback
    // Vac_SetAudioCallback(audio_callback);

    Apu_Reset();
}

void Apu_Reset()
{
#ifdef DEBUG
    CHECK_INIT
#endif
    apuflags = 0;

    // reset channels
    memset(&pulse[0], 0, sizeof(pulse_channel_t));
    memset(&pulse[1], 0, sizeof(pulse_channel_t));
    memset(&triangle, 0, sizeof(triangle_channel_t));
    memset(&noise, 0, sizeof(noise_channel_t));
    noise.shift_reg = 0x01;

    // TODO: Turn off channels while figuring this out...
    // pulse[0].mute = true;
    // pulse[1].mute = true;
    // triangle.mute = true;
    noise.mute = true;

    // reset ring buffer
    memset(audio_buf, 0, AUDIO_BUFFER_SIZE * sizeof(float));
}

void Apu_Step(int cycle_budget, u32 keystate)
{
#ifdef DEBUG
    CHECK_INIT
#endif
    // debug mute channels
    static unsigned int last_ms = 0;
    if (Vac_MsPassedFrom(last_ms) >= 200) {
        if (keystate & KEY_MUTE_1) {
            pulse[0].mute = !pulse[0].mute;
            last_ms = Vac_Now();
        }
        if (keystate & KEY_MUTE_2) {
            pulse[1].mute = !pulse[1].mute;
            last_ms = Vac_Now();
        }
        if (keystate & KEY_MUTE_3) {
            triangle.mute = !triangle.mute;
            last_ms = Vac_Now();
        }
    }

    // The cpu clocks at about 1.789 Mhz (cycles per sec)
    // The sample rate is 44.1 Khz (samples per sec)
    // Thus we need 1.789 / .0441 = 40.5 (cycles per sample)
    // The APU runs about half the speed so we really need 40.5 / 2 cycles per sample
    // So about every 41 cycles or so we generate a sample 
    // and add it to the ring buffer
    // Thanks to this nesdev post for the strategy:
    // https://forums.nesdev.com/viewtopic.php?f=5&t=15383

    // state vars
    static int cycle = 0;

    int abuf_cursor = 0;
    for (int i = 0; i < cycle_budget; i++) {
        if (cycle % 20 == 0) {
            float sample = 0;
            sample += gen_pulse_sample(0);
            sample += gen_pulse_sample(1);
            sample += gen_triangle_sample();
            sample += gen_noise_sample();
            audio_buf[abuf_cursor] = sample;
            abuf_cursor++;
            if (abuf_cursor >= AUDIO_BUFFER_SIZE) {
                ERROR("Out of AUDIO Buffer!");
                EXIT(1);
            }
        }

        // quarter frame
        if (cycle == 3728 || cycle == 7456 || cycle == 11185 || cycle == 14914 || cycle == 18640) {
            // clock envelope and triangle lin counter
            if (!(cycle == 14914 && counter_mode == COUNTER_5STEP)) {
                // pulse channels
                for (int channel = 0; channel < 2; channel++) {
                    if (!pulse[channel].const_vol && pulse[channel].volume > 0) {
                        pulse[channel].volume--;
                    }
                }

                // triangle lin counter
                if (triangle.reload) {
                    triangle.lin_counter = triangle.lin_counter_reload;
                } else if (triangle.lin_counter > 0) {
                    triangle.lin_counter--;
                }

                if (!triangle.halt_counter) {
                    triangle.reload = false;
                }
            }

            // half frame
            if (cycle == 7456 || (cycle == 14914 && counter_mode == COUNTER_4STEP) 
                || (cycle == 18640 && counter_mode == COUNTER_5STEP)) {
                // clock len counters and sweep
                for (int channel = 0; channel < 2; channel++) {
                    if (pulse[channel].counter == 0) {
                        // mute
                        pulse[channel].enabled = false;
                    } else if (!pulse[channel].halt_counter) {
                        pulse[channel].counter--;
                    }

                    // sweep
                    if (pulse[channel].sweep.on) {
                        u8 change = pulse[channel].timer >> pulse[channel].sweep.shift;
                        // negate if needed
                        change = pulse[channel].sweep.negate ? ~change + 1 : change;
                        // pulse[0] should use 1's complement for some reason :/
                        if (channel == 0) {
                            change--;
                        }
                        pulse[channel].timer += change;

                        // mute channel on big period
                        if (pulse[channel].timer > 0x7FF) {
                            pulse[channel].enabled = false;
                            pulse[channel].counter = 0;
                        }
                    }
                }

                // noise counter
                if (noise.counter == 0) {
                    // mute
                    noise.enabled = false;
                } else if (!noise.halt_counter) {
                    noise.counter--;
                }
            }

            if (!triangle.lin_counter || !triangle.counter) {
                triangle.enabled = false;
            }

            if (cycle == 14914 && COUNTER_4STEP && !irq_disabled) {
                ERROR("HEY THIS HAS AN INTERRUPT REMEMBER TO COME AND IMPLEMENT THIS\n");
                EXIT(1);
                Cpu_Irq();
            }
        }


        // increment cycle (magic numbers from here: 
        // https://wiki.nesdev.com/w/index.php/APU_Frame_Counter)
        cycle = (cycle + 1) % (counter_mode == COUNTER_5STEP ? 18640 : 14914);
    }

    // queue audio samples
    Vac_QueueAudio(audio_buf, abuf_cursor * sizeof(float));
}

u8 Apu_Read(u16 addr)
{
#ifdef DEBUG
    CHECK_INIT
#endif

    u8 data = 0;
    switch (addr) {
    case 0x4015: // Status Flags
        if (pulse[0].counter > 0) data |= FLAGS_PULSE1;
        if (pulse[1].counter > 0) data |= FLAGS_PULSE2;
        // TODO: Triangle, Noise, DMC

        // clear frame interrupt flag
        apuflags &= ~FLAGS_FRAME_INT;
        break;
    default:
        WARNING("Read support not available for $%04X\n", addr);
        break;
    }

    return data;
}

void Apu_Write(u8 data, u16 addr)
{
#ifdef DEBUG
    CHECK_INIT
#endif

    int channel = addr & 0x0004 ? 1 : 0;
    switch (addr) {
    // Duty and Volume controls
    case 0x4000: // pulse 1
    case 0x4004: // pulse 2
        pulse[channel].halt_counter = (data & 0x20) != 0;
        switch (data >> 6) {
        case 0b00:
            pulse[channel].duty = 0.125;
            break;
        case 0b01:
            pulse[channel].duty = 0.25;
            break;
        case 0b10:
            pulse[channel].duty = 0.50;
            break;
        case 0b11:
            pulse[channel].duty = 0.75;
            break;
        }
        pulse[channel].const_vol = (data & 0x10) != 0;
        pulse[channel].volume = data & 0x0F;
        break;
    // Sweep envelope
    case 0x4001: // pulse 1
    case 0x4005: // pulse 2
        pulse[channel].sweep.on = (data >> 7) & 0x1;
        pulse[channel].sweep.period = (data >> 4) & 0x7;
        pulse[channel].sweep.negate = (data >> 3) & 0x1;
        pulse[channel].sweep.shift = data & 0x7;
        break;
    // Timer Low
    case 0x4002: // pulse 1
    case 0x4006: // pulse 2
        pulse[channel].timer = (pulse[channel].timer & 0xFF00) | data;
        break;
    // Timer High and len counter
    case 0x4003: // pulse 1
    case 0x4007: // pulse 2
        pulse[channel].timer = (pulse[channel].timer & 0x00FF) | ((data & 0x7) << 8);
        pulse[channel].counter = len_table[(data >> 3) & 0x1F];
        pulse[channel].enabled = true;
        pulse[channel].warm_up = 0;
        // Reset phase
        pulse[channel].t_phase = 0;
        break;
    case 0x4008: // Triangle
        triangle.lin_counter_reload = data & 0x7F;
        triangle.halt_counter = (data >> 7) & 0x1;
        break;
    case 0x400A: // Triangle
        triangle.timer = (triangle.timer & 0xFF00) | data;
        break;
    case 0x400B: // Triangle
        triangle.timer = (triangle.timer & 0x00FF) | ((data & 0x7) << 8);
        triangle.counter = len_table[(data >> 3) & 0x1F]; // TODO???
        triangle.enabled = true;
        triangle.reload = true;
        triangle.warm_up = 0;
        triangle.warm_up_step = 1;
        break;
    case 0x400C: // Noise
        noise.halt_counter = (data & 0x20) != 0;
        noise.const_vol = (data & 0x10) != 0;
        noise.volume = data & 0x0F;
        break;
    case 0x400E: // Noise
        noise.mode = (data & 0x80) != 0;
        // TODO: period
        break;
    case 0x400F: // Noise
        noise.counter = (data >> 3) & 0x1F;
        noise.enabled = true;
        break;
    case 0x4015: // Status Flags
        apuflags = data;
        if (!(apuflags & FLAGS_PULSE1)) {
            // TODO: silence pulse 1
            pulse[0].enabled = false;
        }
        if (!(apuflags & FLAGS_PULSE2)) {
            // TODO: silence pulse 2
            pulse[1].enabled = false;
        }
        if (!(apuflags & FLAGS_TRIANGLE)) {
            // TODO: silence triangle
            triangle.enabled = false;
        }
        if (!(apuflags & FLAGS_NOISE)) {
            // TODO: silence noise
            noise.enabled = false;
        }
        if (!(apuflags & FLAGS_DMC)) {
            // TODO: silence DMC
        }
        break;
    case 0x4017: // Frame Counter
        irq_disabled = (data & 0x40) != 0;
        counter_mode = (data & 0x80) ? COUNTER_5STEP : COUNTER_4STEP;
        break;
    default:
        WARNING("Write support not available for $%04X\n", addr);
        break;
    }
}
