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

extern void matrix_shift(ws2811_led_t *matrix, const uint32_t matrix_length, const int direction);
extern void matrix_rotate(ws2811_led_t *matrix, const uint32_t matrix_length, const int direction);
extern void matrix_to_ledstring(ws2811_led_t *matrix, ws2811_led_t *ledstring, uint32_t length, bool invert);


/* Circularly linked list. */
struct advanced_led {
    /* This doesn't change, as it is the base color */
    ws2811_led_t base_color;
    ws2811_led_t color; /* Base Color * brightness/100 */
    uint32_t brightness;
    struct advanced_led *next;
};

static struct advanced_led *head = NULL;
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

uint32_t get_red(ws2811_led_t color)
{
    return (color & 0xFF0000) >> 16;
}

uint32_t get_green(ws2811_led_t color)
{
    return (color & 0x00FF00) >> 8;
}

uint32_t get_blue(ws2811_led_t color)
{
    return (color & 0x0000FF);
}

ws2811_led_t
get_ws2811_led_color(uint32_t red, uint32_t green, uint32_t blue)
{
    return ((ws2811_led_t)(red << 16) + (ws2811_led_t)(green << 8) + (ws2811_led_t)blue);
}

ws2811_led_t
get_ws2811_led_with_brightness(ws2811_led_t color, uint32_t brightness)
{
    double red = (double) get_red(color);
    double green = (double)get_green(color);
    double blue = (double)get_blue(color);
    red   = red * (double)((double)brightness/100.0);
    green = green * (double)((double)brightness/100.0);
    blue  = blue * (double)((double)brightness/100.0);
    return get_ws2811_led_color((uint32_t)red, (uint32_t)green, (uint32_t)blue);
}

void
clear_advanced_led()
{
    struct advanced_led *curr = head->next;
    head->next = NULL;

    do {
        struct advanced_led *next = curr->next;
        free(curr);
        curr = next;
    }
    while (curr->next != NULL);
    free(curr);
    head = NULL;
}

/* Always inserts at end. Un-ordered list */
void insert_color(ws2811_led_t color, uint32_t brightness)
{
    struct advanced_led *newLed = calloc(1, sizeof(struct advanced_led));
    newLed->base_color = color;
    newLed->brightness = brightness;
    newLed->color = get_ws2811_led_with_brightness(newLed->base_color, newLed->brightness);
    newLed->next = NULL;

    /* Nothing in list yet */
    if (head == NULL) {
        head = newLed;
        newLed->next = head;
    }
    else {
        struct advanced_led *curr = head;
        /* Start at curr == HEAD */
        while (curr->next != head) {
            curr = curr->next;
        }
        newLed->next = head;
        curr->next = newLed;
    }
}

void
advanced_led_to_matrix(ws2811_led_t *matrix)
{
    struct advanced_led *curr = head;
    uint32_t matrix_index = 0;
    do {
        memcpy(&matrix[matrix_index++], &curr->color, sizeof(ws2811_led_t));
        //matrix_index++;
        curr = curr->next;
    } while (curr->next != head);
}

void
initialize_advanced_led(struct pattern *pattern)
{
    uint32_t color = 0;
    // For each color
    for (uint32_t i = 0; i < pattern->matrix_length; i++) {
        // for each led in pulseWidth
        for (uint32_t j = 0; j < pattern->pulseWidth; j++) {
            if (i == pattern->matrix_length) {
                break;
            }
            insert_color(colors[color], pattern->max_brightness);
            i++;
        }
        i--; // XXX: gross
        color = (color < 7) ? color + 1 : 0;
    }

    /* Fill in the end */
    if (pattern->matrix_length % pattern->pulseWidth != 0) {
        insert_color(colors[color], pattern->max_brightness);
    }
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
    assert(pattern->matrix_length > pattern->pulseWidth);
    assert(pattern->pulseWidth != 0);

    /* Give the array it's starting colors */
    initialize_advanced_led(pattern);

    /* Convert from advanced led to a matrix */
    advanced_led_to_matrix(pattern->matrix);
    
    /* Or ignore matrix entirely and ONLY use advanced_led */
    matrix_to_ledstring(pattern->ledstring->channel[0].leds,
                        pattern->matrix,
                        pattern->ledstring->channel[0].count,
                        false);

    matrix_to_ledstring(pattern->ledstring->channel[1].leds,
                        pattern->matrix + pattern->ledstring->channel[0].count,
                        pattern->ledstring->channel[1].count,
                        true);

    if ((ret = ws2811_render(pattern->ledstring)) != WS2811_SUCCESS) {
        log_error("ws2811_renderer failed: %s", ws2811_get_return_t_str(ret));
        // XXX: This should cause some sort of fatal error to propogate upwards
        return NULL;
    }

    while (pattern->running)
    {
        /* Starts with advanced led's all full. First iteration is head, then head moves forward one.
         * Circularly linked list;
         */
        if (newColor) {
            /* Bright up one color at a time, through whole strip */
            struct advanced_led *curr = head;
            do {
                if ((color == colors[COLOR_NONE]) || curr->base_color == color) {
                    curr->brightness = intensity;
                    curr->color = get_ws2811_led_with_brightness(curr->base_color, curr->brightness);
                    printf("%d %d\n", curr->color, curr->brightness);
                }
                curr = curr->next;
            }
            while (curr->next != head);
            newColor = false;
        }
        
        /* Rotate head */
        head = head->next;

        /* Convert from advanced led to a matrix */
        advanced_led_to_matrix(pattern->matrix);

        matrix_to_ledstring(pattern->ledstring->channel[0].leds,
                            pattern->matrix, 
                            pattern->ledstring->channel[0].count,
                            false);

        matrix_to_ledstring(pattern->ledstring->channel[1].leds,
                            pattern->matrix + pattern->ledstring->channel[0].count,
                            pattern->ledstring->channel[1].count,
                            true);


        if ((ret = ws2811_render(pattern->ledstring)) != WS2811_SUCCESS) {
            log_error("ws2811_renderer failed: %s", ws2811_get_return_t_str(ret));
            // XXX: This should cause some sort of fatal error to propogate upwards
            return NULL;
        }
        usleep(1000000 / pattern->movement_rate);
    }

    if (ret == WS2811_SUCCESS) {
        return NULL;
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
    //pattern->name = calloc(256, sizeof(char));
    strcpy(pattern->name, "Perimeter Rainbow");
    //pattern->name = "Perimiter Rainbow";
    pattern->matrix_length = pattern->ledstring->channel[0].count + pattern->ledstring->channel[1].count;
    pattern->matrix = calloc(pattern->matrix_length, sizeof(ws2811_led_t));
    return WS2811_SUCCESS;
}   

ws2811_return_t
perimeter_rainbow_delete(struct pattern *pattern)
{
    log_trace("perimeter_rainbow_delete()");
    log_debug("Pattern %s: Freeing objects", pattern->name);
    clear_advanced_led();
    //free(pattern->name);
    //pattern->name = NULL;
    free(pattern->matrix);
    pattern->matrix = NULL;
    pattern->matrix_length = 0;
    return WS2811_SUCCESS;
}
