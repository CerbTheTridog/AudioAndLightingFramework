#ifndef __PATTERN_H__
#define __PATTERN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rpi_ws281x/ws2811.h"
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include "log.h"
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN_ONE            18
#define GPIO_PIN_TWO            13
#define DMA                     10
#define STRIP_TYPE              WS2811_STRIP_GRB		// WS2812/SK6812RGB integrated chip+leds

#define COLOR_RED         0x00FF0000
#define COLOR_ORANGE      0x00FF8000
#define COLOR_YELLOW      0x00FFFF00
#define COLOR_GREEN       0x0000FF00
#define COLOR_LIGHTBLUE   0x0000FFFF
#define COLOR_BLUE        0x000000FF
#define COLOR_PURPLE      0x00FF00FF
#define COLOR_PINK        0x00FF0080
/* Basically means "Ignore color */
#define COLOR_NONE        0x00000000
static const ws2811_led_t colors[] =
{
    COLOR_RED,
    COLOR_ORANGE,
    COLOR_YELLOW,
    COLOR_GREEN,
    COLOR_LIGHTBLUE,
    COLOR_BLUE,
    COLOR_PURPLE,
    COLOR_PINK,
    COLOR_NONE
};

#define MAX_NAME_LENGTH 256
static const uint32_t colors_size = 8;

/* The basic structure for all patterns */
struct pattern
{
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

    /* The maximum brightness of the strip */
    uint32_t max_brightness;

    /* The thread id of the running loop */
    pthread_t thread_id;

    /* The actual led string */
    ws2811_t *ledstring;

    /* The 2-dimensional representation of what lights are what color */
    ws2811_led_t *matrix;
    uint32_t matrix_length;

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

    char name[256];
};

static inline struct pattern*
create_pattern()
{

    struct pattern *ret = (struct pattern*)calloc(1, sizeof(struct pattern));
    // XXX: Clean this up!
    ret->max_brightness = 100;
    return ret;
}

static inline void
pattern_delete(struct pattern *pattern)
{
    free(pattern->ledstring);
    pattern->ledstring = NULL;
}

static inline ws2811_return_t
configure_ledstring_single(struct pattern *pattern, uint32_t led_count)
{
    ws2811_return_t ret = WS2811_SUCCESS;

    pattern->ledstring = (ws2811_t*)calloc(1, sizeof(ws2811_t));
    if (pattern->ledstring == NULL) {
        return WS2811_ERROR_OUT_OF_MEMORY;
    }

    pattern->ledstring->freq = TARGET_FREQ;
    pattern->ledstring->dmanum = DMA;
    pattern->ledstring->channel[0].gpionum = GPIO_PIN_ONE;
    pattern->ledstring->channel[0].count = led_count;
    pattern->ledstring->channel[0].invert = 0;
    /* The strip brightness is irrelevent to pattern brightness */
    pattern->ledstring->channel[0].brightness = 100;
    pattern->ledstring->channel[0].strip_type = STRIP_TYPE;
    pattern->ledstring->channel[1].gpionum = 0;
    pattern->ledstring->channel[1].count = 0;
    pattern->ledstring->channel[1].invert = 0;
    pattern->ledstring->channel[1].brightness = 100;
    pattern->ledstring->channel[1].strip_type = STRIP_TYPE;

    if ((ret = ws2811_init(pattern->ledstring)) != WS2811_SUCCESS) {
        log_fatal("ws2811_init failed: %s", ws2811_get_return_t_str(ret));
        return ret;
    }
    return ret;
}

static inline ws2811_return_t
configure_ledstring_double(struct pattern *pattern, uint32_t ch1_led_count, uint32_t ch2_led_count)
{
    ws2811_return_t ret = WS2811_SUCCESS;

    pattern->ledstring = (ws2811_t*) calloc(1, sizeof(ws2811_t));
    if (pattern->ledstring == NULL) {
        return WS2811_ERROR_OUT_OF_MEMORY;
    }

    pattern->ledstring->freq = TARGET_FREQ;
    pattern->ledstring->dmanum = DMA;
    pattern->ledstring->channel[0].gpionum = GPIO_PIN_ONE;
    pattern->ledstring->channel[0].count = ch1_led_count;
    pattern->ledstring->channel[0].invert = 0;
    pattern->ledstring->channel[0].brightness = 100;
    pattern->ledstring->channel[0].strip_type = STRIP_TYPE;

    pattern->ledstring->channel[1].gpionum = GPIO_PIN_TWO;
    pattern->ledstring->channel[1].count = ch2_led_count;
    pattern->ledstring->channel[1].invert = 0;
    pattern->ledstring->channel[1].brightness = 100;
    pattern->ledstring->channel[1].strip_type = STRIP_TYPE;

    if ((ret = ws2811_init(pattern->ledstring)) != WS2811_SUCCESS) {
        log_fatal("ws2811_init failed: %s", ws2811_get_return_t_str(ret));
        return ret;
    }
    return ret;
}


/* This only shifts forward one */
inline void
move_lights(struct pattern *pattern, uint32_t shift_distance)
{
    ws2811_led_t *ch1_array = pattern->ledstring->channel[0].leds;
    ws2811_led_t *ch2_array = pattern->ledstring->channel[1].leds;

    uint32_t count_ch1 = pattern->ledstring->channel[0].count;
    int i = count_ch1 - shift_distance;
    while (i > 0) {
        memmove(&ch1_array[i], &ch1_array[i-shift_distance], sizeof(ws2811_led_t));
        i--;
    }

    uint32_t count_ch2 = pattern->ledstring->channel[1].count;
    int j = count_ch2 - shift_distance;
    while (j > 0) {
        memmove(&ch2_array[j], &ch2_array[j-shift_distance], sizeof(ws2811_led_t));
        j--;
    }
}

inline void
matrix_shift(ws2811_led_t *matrix, const uint32_t matrix_length, const int direction)
{
    if (direction == -1) {
        for (uint32_t i = 1; i < matrix_length; i++) {
            memmove(&matrix[i-1], &matrix[i], sizeof(ws2811_led_t));
            matrix[i-1] = matrix[i];
        }
    }
    else if (direction == 1) {
        uint32_t i = matrix_length -1;
        while (i > 0) {
            memmove(&matrix[i], &matrix[i-1], sizeof(ws2811_led_t));
            i--;
        }
        memmove(&matrix[i], &matrix[1], sizeof(ws2811_led_t));
    }
}

inline void
matrix_rotate(ws2811_led_t *matrix, const uint32_t matrix_length, const int direction)
{
    if (direction == -1) {
        ws2811_led_t temp = matrix[0];
        matrix_shift(matrix, matrix_length, direction);
        matrix[matrix_length-1] = temp;
    }
    else if (direction == 1) {
        ws2811_led_t temp = matrix[matrix_length-1];
        matrix_shift(matrix, matrix_length, direction);
        matrix[0] = temp;
    }
}

/* destination, source, length, invert */
inline void
matrix_to_ledstring(ws2811_led_t *ledstring, ws2811_led_t *matrix, uint32_t length, bool invert)
{
    if (invert) {
        uint32_t i = length-1;
        uint32_t j = 0;
        while (i > 0) {
            memmove(&ledstring[j], &matrix[i], sizeof(ws2811_led_t));
            i--;
            j++;
        }
        memmove(&ledstring[j], &matrix[i], sizeof(ws2811_led_t));
    }
    else {
        for (uint32_t i = 0; i < length; i++) {
            memmove(&ledstring[i], &matrix[i], sizeof(ws2811_led_t));
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif
