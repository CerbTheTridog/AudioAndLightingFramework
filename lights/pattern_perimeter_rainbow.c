/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>
#include <pthread.h>
#include <assert.h>

#include "rpi_ws281x/clk.h"
#include "rpi_ws281x/gpio.h"
#include "rpi_ws281x/dma.h"
#include "rpi_ws281x/pwm.h"
#include "version.h"

#include "rpi_ws281x/ws2811.h"
#include "pattern_perimeter_rainbow.h"
#include "log.h"

static ws2811_led_t color = 0;
static bool newColor = false;
static uint32_t intensity = 0;

/* A new color has been injected. What happens to old color? */
ws2811_return_t
perimeter_rainbow_inject(ws2811_led_t in_color, uint32_t in_intensity)
{
    log_trace("pulse_inject(): %d, %d\n", in_color, in_intensity);
    ws2811_return_t ret = WS2811_SUCCESS;
    color = in_color;
    newColor = true;
    intensity = in_intensity;
    return ret;
}

/* Run the threaded loop */
void *
matrix_run3(void *vargp)
{
    log_matrix_trace("matrix_run3()");
    ws2811_return_t ret = WS2811_SUCCESS;
    struct pattern *pattern = (struct pattern*)vargp;
    assert(pattern->running);

    // First, fill initial pattern
    uint32_t led, color, width;
    color = 0;
    uint32_t led_count_ch1 = pattern->ledstring->channel[0].count;
    //uint32_t led_count_ch2 = pattern->ledstring->channel[1].count;
    assert(led_count_ch1 > pattern->pulseWidth);
    assert(pattern->pulseWidth != 0);
    // XXX: FIX led_count_ch2
    for (led = 0; led < led_count_ch1; ) {
        for (width = 0; width < pattern->pulseWidth; width++) {
            if (led == led_count_ch1) {
                break;
            }
            pattern->ledstring->channel[0].leds[led] = colors[color];   
            pattern->ledstring->channel[1].leds[led] = colors[color];
            led++;
        }
        color = (color < 7) ? color + 1: 0;

    }

    if ((ret = ws2811_render(pattern->ledstring)) != WS2811_SUCCESS) {
        log_error("ws2811_renderer failed: %s", ws2811_get_return_t_str(ret));
        // XXX: This should cause some sort of fatal error to propogate upwards
        return NULL;
    }

    while (pattern->running)
    {   
        ws2811_led_t buffer1 = pattern->ledstring->channel[0].leds[led_count_ch1-1];
        ws2811_led_t buffer2 = pattern->ledstring->channel[1].leds[led_count_ch1-1];
        move_lights(pattern, 1);
        pattern->ledstring->channel[0].leds[0] = buffer1;
        pattern->ledstring->channel[1].leds[0] = buffer2;
        if ((ret = ws2811_render(pattern->ledstring)) != WS2811_SUCCESS) {
            log_error("ws2811_renderer failed: %s", ws2811_get_return_t_str(ret));
            // XXX: This should cause some sort of fatal error to propogate upwards
            return NULL;
        }
        usleep(1000000 / pattern->movement_rate);
    }

    return NULL;
}

/* Initialize everything, and begin the thread */
ws2811_return_t
perimeter_rainbow_load(struct pattern *pattern)
{
    log_trace("perimeter_rainbow_load()");

    /* A protection against matrix_run() being called in a bad order. */
    pattern->running = 1;

    pthread_create(&pattern->thread_id, NULL, matrix_run3, (void*)pattern);
    log_info("Pattern %s: Loop is now running.", pattern->name);
    return WS2811_SUCCESS;
}

ws2811_return_t
perimeter_rainbow_start(struct pattern *pattern)
{
    log_trace("perimeter_rainbow_start()");
    pattern->paused = false;
    return WS2811_SUCCESS;
}

ws2811_return_t
perimeter_rainbow_stop(struct pattern *pattern)
{
    log_trace("perimeter_rainbow_stop()");
    pattern->paused = true;
    //matrix_clear(pattern);
    return WS2811_SUCCESS;
}

ws2811_return_t
perimeter_rainbow_clear(struct pattern *pattern)
{
    log_trace("perimeter_rainbow_clear()\n");
    log_info("Pattern %s: Clearing Pattern", pattern->name);
    ws2811_return_t ret = WS2811_SUCCESS;
    uint32_t i;
    uint32_t led_count_ch1 = pattern->ledstring->channel[0].count;
    uint32_t led_count_ch2 = pattern->ledstring->channel[1].count;
    for (i = 0; i < led_count_ch1; i++) {
        pattern->ledstring->channel[0].leds[i] = 0;
    }
    for (i = 0; i < led_count_ch2; i++) {
        pattern->ledstring->channel[1].leds[i] = 0;
    }

    if ((ret = ws2811_render(pattern->ledstring)) != WS2811_SUCCESS) {
        log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
        // xxx: this should cause some sort of fatal error to propogate upwards
    }
    return ret;
}

ws2811_return_t
perimeter_rainbow_pause(struct pattern *pattern)
{
    log_trace("perimeter_rainbow_pause()");
    pattern->paused = true;
    return WS2811_SUCCESS;
}

/* Turn off strip, kill second process */
ws2811_return_t
perimeter_rainbow_kill(struct pattern *pattern)
{
    log_trace("perimeter_rainbow_kill()");
    log_debug("Pattern %s: Stopping run", pattern->name);
    pattern->running = 0;

    log_debug("Pattern %s: Loop Waiting for thread %d to end", pattern->name, pattern->thread_id);
    pthread_join(pattern->thread_id, NULL);

    if (pattern->clear_on_exit) {
        perimeter_rainbow_clear(pattern);
    }

    log_info("Pattern %s: Loop now stopped", pattern->name);
    return WS2811_SUCCESS;
}

ws2811_return_t
perimeter_rainbow_create(struct pattern *pattern)
{
    log_trace("perimeter_rainbow_create()");
    /* Assign function pointers */
    pattern->func_load_pattern = &perimeter_rainbow_load;
    pattern->func_start_pattern = &perimeter_rainbow_start;
    pattern->func_kill_pattern = &perimeter_rainbow_kill;
    pattern->func_pause_pattern = &perimeter_rainbow_pause;
    pattern->func_inject = &perimeter_rainbow_inject;

    /* Set default values */
    pattern->running = true;
    pattern->paused = true;
    pattern->pulseWidth = 0;
    pattern->name = calloc(256, sizeof(char));
    strcpy(pattern->name, "Perimiter Rainbow");
    //(*pattern)->name = "Perimeter Rainbow";
    return WS2811_SUCCESS;
}   

ws2811_return_t
perimeter_rainbow_delete(struct pattern *pattern)
{
    log_trace("perimeter_rainbow_delete()");
    log_debug("Pattern %s: Freeing objects", pattern->name);
    free(pattern->name);
    pattern->name = NULL;
    return WS2811_SUCCESS;
}
