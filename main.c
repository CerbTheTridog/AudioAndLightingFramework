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


static char VERSION[] = "0.0.6";

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


#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"

#include "ws2811.h"
#include "pattern.h"
#include "pattern_rainbow.h"
#include "pattern_pulse.h"
#include "pattern_perimeter_rainbow.h"
#include "pattern_static_color.h"
#include "log.h"

/*
	PWM0, which can be set to use GPIOs 12, 18, 40, and 52.
	Only 12 (pin 32) and 18 (pin 12) are available on the B+/2B/3B
	PWM1 which can be set to use GPIOs 13, 19, 41, 45 and 53.
	Only 13 is available on the B+/2B/PiZero/3B, on pin 33
	PCM_DOUT, which can be set to use GPIOs 21 and 31.
	Only 21 is available on the B+/2B/PiZero/3B, on pin 40.
	SPI0-MOSI is available on GPIOs 10 and 38.
	Only GPIO 10 is available on all models.

	The library checks if the specified gpio is available
	on the specific model (from model B rev 1 till 3B)

*/


#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

#define SLEEP                   .5
#define LED_COUNT               750
#define MOVEMENT_RATE           100
#define PULSE_WIDTH             10

static int clear_on_exit = 0;
static struct pattern *pattern;
static double movement_rate = MOVEMENT_RATE;
static bool maintain_colors = false;
static uint32_t pulse_width = PULSE_WIDTH;
int program = 0;
static uint32_t sleep_rate = SLEEP * 1000000;
uint8_t running = 1;

static void ctrl_c_handler(int signum)
{
    log_info("Control+C GET!");
	(void)(signum);
    running = 0;
    pattern->func_kill_pattern(pattern);
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}


void parseargs(int argc, char **argv)
{
	int index;
	int c;

	static struct option longopts[] =
	{
		{"help", no_argument, 0, 'h'},
		{"clear", no_argument, 0, 'c'},
		{"version", no_argument, 0, 'v'},
        {"program", required_argument, 0, 'p'},
        {"movement_rate", required_argument, 0, 'm'},
        {"maintain_color", required_argument, 0, 'M'},
        {"sleep_rate", required_argument, 0, 'S'},
        {"pulse_width", required_argument, 0, 'P'},
        {0, 0, 0, 0}
	};

	while (1)
	{

		index = 0;
		c = getopt_long(argc, argv, "hcv:p:m:M:S:P:", longopts, &index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* handle flag options (array's 3rd field non-0) */
			break;

		case 'h':
			printf("FARTING");
            fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			fprintf(stderr, "Usage: %s \n"
				"-h (--help)    - this information\n"
				"-c (--clear)   - clear matrix on exit.\n"
				"-v (--version) - version information\n"
                "-p (--program) - Which program to run\n"
                "-m (--movement_rate)  - The number of seconds for an LED to move from one to the next\n"
                "-S (--sleep_rate)     - The number of seconds to sleep between commands\n"
                "###-M### (--maintain_color) - Goes nowhere, does nothing\n"
                "-P (--pulse_width)    - The number of LEDs x2 per pulse\n"
				, argv[0]);
			exit(-1);

        case 'm':
            if (optarg) {
                movement_rate = atof(optarg);
            }
            break;
        case 'M':
            if (optarg) {
                maintain_colors = atoi(optarg);
            }
            break;
		case 'c':
			clear_on_exit=1;
			break;

		case 'p':
            if (optarg) {
                program = atoi(optarg);
            }
            break;
        case 'P':
            if (optarg) {
                pulse_width = atoi(optarg);
            }
            break;
        case 'S':
            if (optarg) {
                sleep_rate = atof(optarg) * 1000000;
                printf("SLEEPING FOR %d\n", sleep_rate);
            }
            break;
		case 'v':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			exit(-1);

		case '?':
			/* getopt_long already reported error? */
			exit(-1);
		default:
			exit(-1);
		}
	}
}

int main(int argc, char *argv[])
{
    /* LOG_MATRIX_TRACE, LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL */
    log_set_level(LOG_DEBUG);

    ws2811_return_t ret;
    log_info("Version: %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

    parseargs(argc, argv);
    /* Handlers should only be caught in this file. And commands propogate down */
    setup_handlers();
    pattern = create_pattern();

    /* Which pattern to do? */
    if (program == 0) {
        if ((ret = configure_ledstring_single(pattern, LED_COUNT)) != WS2811_SUCCESS) {
            log_fatal("Bad stuff");
            return ret;
        }
        if((ret = rainbow_create(pattern)) != WS2811_SUCCESS) {
            log_fatal("rainbox_create failed: %s", ws2811_get_return_t_str(ret));
            return ret;
        }
    }
    else if (program == 1) {
        if ((ret = configure_ledstring_single(pattern, LED_COUNT)) != WS2811_SUCCESS) {
            log_fatal("Bad stuff");
            return ret;
        }

        if ((ret = pulse_create(pattern)) != WS2811_SUCCESS) {
            log_fatal("pulse_create failed: %s", ws2811_get_return_t_str(ret));
            return ret;
        }

        pattern->pulseWidth = pulse_width;
    }
    else if (program == 2) {
        if ((ret = configure_ledstring_double(pattern, LED_COUNT, LED_COUNT)) != WS2811_SUCCESS) {
            log_fatal("Bad stuff");
            return ret;
        }
        if ((ret = perimeter_rainbow_create(pattern)) != WS2811_SUCCESS) {
            log_fatal("perimeter_rainbow_create failed: %s", ws2811_get_return_t_str(ret));
            return ret;
        }
        pattern->pulseWidth = pulse_width;
    }
    else if (program == 3) {
        if ((ret = configure_ledstring_double(pattern, LED_COUNT, LED_COUNT)) != WS2811_SUCCESS) {
            log_fatal("Bad stuff");
            return ret;
        }
        if ((ret = static_color_create(pattern)) != WS2811_SUCCESS) {
            log_fatal("static_color_create failed: %s", ws2811_get_return_t_str(ret));
            return ret;
        }
        sleep_rate = 600000;
    }
    pattern->clear_on_exit = clear_on_exit;
    pattern->maintainColor = maintain_colors;
    pattern->movement_rate = movement_rate;
  
    /* Load the program into memory */
    pattern->func_load_pattern(pattern);

    /* Start the program */
    pattern->func_start_pattern(pattern);

    /* We halt until control+c is provided */
    if (program == 0) {
        while (running) {
            sleep(1);
        }
    }
    else if (program == 1) {
        uint32_t i = 0;
        bool random = false;
        while (running) {
            if (random) {
                pattern->func_inject(colors[rand() % colors_size], rand()%100);
                usleep(sleep_rate);
            }
            else {
                pattern->func_inject(colors[i], rand()%100);
                usleep(sleep_rate);
                i = (i == colors_size-1) ? 0 : (i + 1);    
            }
        }
    }
    else if (program == 2) {
        while (running) {
            usleep(sleep_rate);
        }
    }
    else if (program == 3) {
        while (running) {
            pattern->func_inject(colors[rand() % colors_size], rand()%100);
            usleep(sleep_rate);
        }
    }
    /* Clear the program from memory */
    ws2811_fini(pattern->ledstring);

    /* Clean up stuff */
    if (program == 0) {
        rainbow_delete(pattern); 
    }
    else if (program == 1) {
        pulse_delete(pattern);
    }
    else if (program == 1) {
        perimeter_rainbow_delete(pattern);
    }
    pattern_delete(pattern);
    free(pattern);
    return ret;
}
