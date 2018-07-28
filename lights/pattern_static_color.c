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
#include "pattern_static_color.h"
#include "log.h"

#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))
//extern void move_lights(struct pattern *pattern, uint32_t shift_distance);

static ws2811_led_t color = 0;
static uint32_t intensity = 0;
static bool newColor = false;

/* A new color has been injected. What happens to old color? */
ws2811_return_t
static_color_inject(ws2811_led_t in_color, uint32_t in_intensity)
{
    log_trace("static_color_inject(): %d, %d\n", in_color, in_intensity);
    ws2811_return_t ret = WS2811_SUCCESS;
    color = in_color;
    newColor = true;
    intensity = in_intensity;
    return ret;
}


/* Run the threaded loop */
void *
static_color_run(void *vargp)
{
    log_matrix_trace("static_color_run()");

    ws2811_return_t ret;
    struct pattern *pattern = (struct pattern*)vargp;

    while (pattern->running)
    {
        /* If the pattern is paused, we won't update anything */
        if (!pattern->paused) {
            if (newColor) {
                uint32_t led;
                uint32_t led_count_ch1 = pattern->ledstring->channel[0].count;
                for (led = 0; led < led_count_ch1; led++) {
                    pattern->ledstring->channel[0].leds[led] = color;
                }
                uint32_t led_count_ch2 = pattern->ledstring->channel[1].count;
                for (led = 0; led < led_count_ch2; led++) {
                    pattern->ledstring->channel[1].leds[led] = color;
                }
                newColor = false;
            }
            if ((ret = ws2811_render(pattern->ledstring)) != WS2811_SUCCESS)
            {
                log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
                // XXX: This should cause some sort of fatal error to propogate upwards
                break;
            }
        }
        // 15 frames /sec
        usleep(1000000 / pattern->movement_rate);
    }

    return NULL;
}



/* Initialize everything, and begin the thread */
ws2811_return_t
static_color_load(struct pattern *pattern)
{
    log_trace("static_color_load()");

    /* A protection against static_color_run() being called in a bad order. */
    pattern->running = 1;

    pthread_create(&pattern->thread_id, NULL, static_color_run, pattern);
    log_info("Static Color Pattern Loop is now running.");
    return WS2811_SUCCESS;
}

ws2811_return_t
static_color_start(struct pattern *pattern)
{
    log_trace("static_color_start()");
    pattern->paused = false;
    return WS2811_SUCCESS;
}

ws2811_return_t
static_color_stop(struct pattern *pattern)
{
    log_trace("static_color_stop()");
    pattern->paused = true;
    return WS2811_SUCCESS;
}

ws2811_return_t
static_color_pause(struct pattern *pattern)
{
    log_trace("static_color_pause()");
    pattern->paused = true;
    return WS2811_SUCCESS;
}

/* Turn off strip, kill second process */
ws2811_return_t
static_color_kill(struct pattern *pattern)
{
    log_trace("static_color_kill()");

    log_debug("Static Color Pattern Loop: Stopping run");
    pattern->running = 0;

    log_debug("Static Color Pattern Loop: Waiting for thread %d to end", pattern->thread_id);
    pthread_join(pattern->thread_id, NULL);

    if (pattern->clear_on_exit) {
        log_info("Static Color Pattern Loop: Clearing light strip");
        uint32_t led_count_ch1 = pattern->ledstring->channel[0].count;
        uint32_t led;
        for (led = 0; led < led_count_ch1; led++) {
            pattern->ledstring->channel[0].leds[led] = 0;
        }
        uint32_t led_count_ch2 = pattern->ledstring->channel[1].count;
        for (led = 0; led < led_count_ch2; led++) {
            pattern->ledstring->channel[0].leds[led] = 0;
        }
        ws2811_render(pattern->ledstring);
    }

    log_info("Static Color Pattern Loop: now stopped");
    return WS2811_SUCCESS;
}

ws2811_return_t
static_color_create(struct pattern *pattern)
{
    log_trace("static_color_create()");
    pattern->func_load_pattern = &static_color_load;
    pattern->func_start_pattern = &static_color_start;
    pattern->func_kill_pattern = &static_color_kill;
    pattern->func_pause_pattern = &static_color_pause;
    pattern->func_inject = &static_color_inject;
    pattern->running = true;
    pattern->paused = true;
    return WS2811_SUCCESS;
}   

ws2811_return_t
static_color_delete(struct pattern *pattern)
{
    log_trace("static_color_delete()");
    log_debug("Static Color Pattern: Freeing objects");
    assert(pattern != NULL);
    return WS2811_SUCCESS;
}
