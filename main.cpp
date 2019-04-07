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
#include <string>
#include <iostream>
#include "version.h"

#include "ws2811.h"
#include "pattern.h"
#include "pattern_rainbow.h"
#include "pattern_pulse.h"
#include "pattern_perimeter_rainbow.h"
#include "pattern_static_color.h"
#include "log.h"

#include "beatmatch.h"
#include "beatmatchevent.h"
#include "audio/lib/libfft.h"

#include "wifi_plug.h"

extern int turn_on_wifi_plug(const std::string&);
extern int turn_off_wifi_plug(const std::string&);
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
/* 672 per strip in living room */
#define LED_COUNT               300
#define MOVEMENT_RATE           100
#define PULSE_WIDTH             10
//#define WIFI_PLUG               "10.0.0.139"

enum ProgramList {
    PULSE,
    PULSE_AUDIO,
    RAINBOW,
    RAINBOW_AUDIO,
    PERIMETER,
    PERIMETER_AUDIO,
    STATIC,
    STATIC_AUDIO
};

static int clear_on_exit = 1;
static struct pattern *pattern;
static double movement_rate = MOVEMENT_RATE;
static bool maintain_colors = false;
static uint32_t pulse_width = PULSE_WIDTH;
static const std::string wifi_plug_host = "10.0.0.139";

// Do not do anything with power source
static bool power_source = false;
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
    struct sigaction action;
    action.sa_handler = ctrl_c_handler;
    sigemptyset (&action.sa_mask);
    action.sa_flags=0;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
}

void parseargs(int argc, char **argv)
{
	int index;
	int c;

	static struct option longopts[] =
	{
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
        	{"clear", required_argument, 0, 'c'},
        	{"program", required_argument, 0, 'p'},
        {"movement_rate", required_argument, 0, 'm'},
        {"sleep_rate", required_argument, 0, 'S'},
        {"pulse_width", required_argument, 0, 'P'},
        {"power_source", required_argument, 0, 'X'},
        {0, 0, 0, 0}
	};

	while (1)
	{

		index = 0;
		c = getopt_long(argc, argv, "hcv:p:m:S:P:X:", longopts, &index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* handle flag options (array's 3rd field non-0) */
			break;

		case 'h':
            fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			fprintf(stderr, "Usage: %s \n"
				"-h (--help)    - this information\n"
				"-c (--clear) [0,1]   - 0 - Do not clear matrix on exit.\n"
				"-v (--version) - version information\n"
                "-p (--program) - Which program to run\n"
                "-m (--movement_rate)  - The number of seconds for an LED to move from one to the next\n"
                "-S (--sleep_rate)     - The number of seconds to sleep between commands\n"
                "-P (--pulse_width)    - The number of LEDs x2 per pulse\n"
                "-X (--power_source)   -  1) On: Turn the power source on and keep it on\n"
                "                      -  2) Off: Turn the power source off and keep it off\n"
                "                      -  3) Auto: Turn power source on for program, and then turn it off when done\n"
				, argv[0]);
			exit(-1);

        case 'm':
            if (optarg) {
                movement_rate = atof(optarg);
            }
            break;
		case 'c':
			if (optarg) {
                clear_on_exit = atoi(optarg);
                if (clear_on_exit != 0 && clear_on_exit != 1) {
                    printf("Clear on Exit can only be enabled or disabled. Exiting\n");
                    exit(-1);
                }
            }
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
            }
            break;
        case 'X':
            if (optarg) {
                switch (atoi(optarg)) {
                case 1: 
                    printf("Turning on power source and exiting\n");
                    turn_on_wifi_plug(wifi_plug_host);
                    exit(1);
                case 2:
                    printf("Turning off power source and exiting\n");
                    turn_off_wifi_plug(wifi_plug_host);
                    exit(1);
                case 3:
                    printf("Power Source set to automatic\n");
                    power_source = true;
                    break;
                }
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

void bmeColorInject(int color, int intensity)
{
    if (pattern) {
        pattern->func_inject(color, intensity);
    }
}

void bmeStaticColorInject(int color, int intensity)
{
    if (pattern) {
        pattern->func_inject(color, intensity);

    }
}

void bmePerimeterColorInject(int color, int intensity)
{
    if (color) {};
    if (pattern) {
        printf("Intensity: %d\n", intensity);
        pattern->func_inject(colors[COLOR_NONE], intensity);
    }
}


int main(int argc, char *argv[])
{
    BeatMatchEvent *bme;
    /* LOG_MATRIX_TRACE, LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL */
    log_set_level(LOG_DEBUG);

    ws2811_return_t ret;
    log_info("Version: %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

    parseargs(argc, argv);
    /* Handlers should only be caught in this file. And commands propogate down */
    setup_handlers();
    if (program != 7) 
	    pattern = create_pattern();

    /* Turn on power source */
    // XXX: Do this better
    if (power_source)
        turn_on_wifi_plug(wifi_plug_host);

    if ((ret = configure_ledstring_double(pattern, LED_COUNT, 0)) != WS2811_SUCCESS) {
        log_fatal("Bad Stuff");
        return ret;
    }

    /* Which pattern to do? */
    if (program == 0) {
        if((ret = rainbow_create(pattern)) != WS2811_SUCCESS) {
            log_fatal("rainbox_create failed: %s", ws2811_get_return_t_str(ret));
            return ret;
        }
    }
    else if (program == 1 || program == 4) {
        if ((ret = pulse_create(pattern)) != WS2811_SUCCESS) {
            log_fatal("pulse_create failed: %s", ws2811_get_return_t_str(ret));
            return ret;
        }
    }
    else if (program == 2 || program == 5) {
        if ((ret = perimeter_rainbow_create(pattern)) != WS2811_SUCCESS) {
            log_fatal("perimeter_rainbow_create failed: %s", ws2811_get_return_t_str(ret));
            return ret;
        }
    }
    else if (program == 3 || program == 6) {
        if ((ret = static_color_create(pattern)) != WS2811_SUCCESS) {
            log_fatal("static_color_create failed: %s", ws2811_get_return_t_str(ret));
            return ret;
        }
        sleep_rate = 600000;
    }

    pattern->max_brightness = 100;
    pattern->pulseWidth = pulse_width;
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
        uint32_t color = rand() & colors_size;
        uint32_t intensity = 100;
        int step = 1;
        int a = 3;
        while (running) {
            if (a== 0) {
                color = rand() % colors_size;
                intensity = rand()%2;
                if (rand() % 2 == 0) {
                    intensity = 100;
                }
                else {
                    intensity = 50;
                }
            }
            else if (a==1){
                if (intensity == 30) {
                    step = 2;
                }
                else if (intensity == 100) {
                    step = -2;
                }
                intensity += step;
            }
            else if (a==2) {
                color = rand() % colors_size;
                intensity = rand() % 100;
            }
            else if(a==3) {
                ;
            }
            //if (rand() % 25 == 0) {
            //    a = !a;
            //}
            pattern->func_inject(colors[color], intensity);
            usleep(sleep_rate);
        }
    }
    else if (program == 3) {
        uint32_t color;
        uint32_t intensity = 100;
        while (running) {
            pattern->func_inject(colors[COLOR_GREEN], 75);
            color = rand() % colors_size;
                        pattern->func_inject(colors[color], intensity);
            usleep(sleep_rate);
        }
    }
    else if (program == 4) {
        bme = new BeatMatchEvent(48000, 8192, 13, 440, 8, true, NULL, bmeColorInject);
        bme->StartThread();
        while (running) { }
    }
    else if (program == 5) {
        bme = new BeatMatchEvent(48000, 8192, 13, 440, 8, true, NULL, bmePerimeterColorInject);
        bme->StartThread();
        while (running) { }
    }
    else if (program == 6) {
        bme = new BeatMatchEvent(48000, 8192, 13, 440, 8, true, NULL, bmeStaticColorInject);
        bme->StartThread();
        while (running) { }
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
    else if (program == 2) {
        perimeter_rainbow_delete(pattern);
    }
    else if (program == 3 || program == 6) {
        static_color_delete(pattern);
    }
    else if (program == 4) {
        pulse_delete(pattern);
        bme->StopThread();
    }
    else if (program == 5) {
        perimeter_rainbow_delete(pattern);
        bme->StopThread();
    }
    else if (program == 6) {
        static_color_delete(pattern);
        bme->StopThread();
    }
    pattern_delete(pattern);
    free(pattern);

    if (power_source)
        turn_off_wifi_plug(wifi_plug_host);
    return ret;
}
