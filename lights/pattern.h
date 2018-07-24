#ifndef __PATTERN_H
#define __PATTERN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <pthread.h>
#include "rpi_ws281x/ws2811.h"
// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN_ONE            18
#define GPIO_PIN_TWO            12
#define DMA                     10
#define STRIP_TYPE              WS2811_STRIP_GRB		// WS2812/SK6812RGB integrated chip+leds

static inline ws2811_t
get_ledstring_single(uint32_t led_count)
{
    
    ws2811_t ledstring;
    ledstring.freq = TARGET_FREQ;
    ledstring.dmanum = DMA;
    ledstring.channel[0].gpionum = GPIO_PIN_ONE;
    ledstring.channel[0].count = led_count;
    ledstring.channel[0].invert = 0;
    ledstring.channel[0].brightness = 255;
    ledstring.channel[0].strip_type = STRIP_TYPE;
    ledstring.channel[1].gpionum = 0;
    ledstring.channel[1].count = 0;
    ledstring.channel[1].invert = 0;
    ledstring.channel[1].brightness = 255;
    ledstring.channel[1].strip_type = STRIP_TYPE;
    return ledstring;
}

static inline ws2811_t
get_ledstring_double(uint32_t ch1_led_count, uint32_t ch2_led_count)
{

    ws2811_t ledstring_double;
    ledstring_double.freq = TARGET_FREQ;
    ledstring_double.dmanum = DMA;
    ledstring_double.channel[0].gpionum = GPIO_PIN_ONE;
    ledstring_double.channel[0].count = ch1_led_count;
    ledstring_double.channel[0].invert = 0;
    ledstring_double.channel[0].brightness = 255;
    ledstring_double.channel[0].strip_type = STRIP_TYPE;
    ledstring_double.channel[1].gpionum = GPIO_PIN_ONE;
    ledstring_double.channel[1].count = ch2_led_count;
    ledstring_double.channel[1].invert = 0;
    ledstring_double.channel[1].brightness = 255;
    ledstring_double.channel[1].strip_type = STRIP_TYPE;
    return ledstring_double;
}

#define COLOR_RED         0x00FF0000
#define COLOR_ORANGE      0x00FF8000
#define COLOR_YELLOW      0x00FFFF00
#define COLOR_GREEN       0x0000FF00
#define COLOR_LIGHTBLUE   0x0000FFFF
#define COLOR_BLUE        0x000000FF
#define COLOR_PURPLE      0x00FF00FF
#define COLOR_PINK        0x00FF0080
static const ws2811_led_t colors[] =
{
    COLOR_RED,
    COLOR_ORANGE,
    COLOR_YELLOW,
    COLOR_GREEN,
    COLOR_LIGHTBLUE,
    COLOR_BLUE,
    COLOR_PURPLE,
    COLOR_PINK
};

static const uint32_t colors_size = 8;

/* The basic structure for all patterns */
struct pattern
{
    /* x - XXX: Rainbow Specific */
    //uint16_t width;
    /* y - XXX: Rainbow Specific*/
    //uint16_t height;
    /* The total number of LEDs to consider as a part of the pattern */
    uint32_t led_count;
    /* Turn off the lights when exiting */
    bool clear_on_exit;
    /* Movement Rate - LEDs per second */
    double movement_rate;
    /* Program is loaded into memory */
    bool running;
    /* Program is actively paused, but still loaded */
    bool paused;
    /* If no new color added, maintain previous color */
    bool maintainColor;
    /* The width of each pulse */
    uint32_t pulseWidth;
    /* The thread id of the running loop */
    pthread_t thread_id;

    /* The actual led string */
    ws2811_t ledstring;
    /* The 2-dimensional representation of what lights are what color */
    ws2811_led_t *matrix;
    
    /* Load a given pattern and start its threaded loop */
    ws2811_return_t (*func_load_pattern)(struct pattern *pattern);
    /* Begin displaying a given pattern */
    ws2811_return_t (*func_start_pattern)(struct pattern *pattern);
    /* Pause or resume a given pattern, based upon argument "pause" */
    ws2811_return_t (*func_pause_pattern)(struct pattern *pattern);
    /* Kill whatever pattern is running */
    ws2811_return_t (*func_kill_pattern)(struct pattern *pattern);
    /* Free pattern form memory */
    ws2811_return_t (*func_delete)(struct pattern *pattern);
    /* XXX: This should only apply to pattern_pulse */
    ws2811_return_t (*func_inject)(ws2811_led_t color, uint32_t intensity);

    char *name;
};


/* This only shifts forward one */
inline void
move_lights(struct pattern *pattern, uint32_t shift_distance)
{
    ws2811_led_t *led_array = pattern->ledstring.channel[0].leds;

    // Shift everything in ledstring exactly one led forward
    int i = pattern->led_count - shift_distance;
    while (i > 0) {
        memmove(&led_array[i], &led_array[i-shift_distance], sizeof(ws2811_led_t));
        i--;
    }
}

/* XXX: Build a move_lights that moves from end back to beginning */

#ifdef __cplusplus
}
#endif

#endif
