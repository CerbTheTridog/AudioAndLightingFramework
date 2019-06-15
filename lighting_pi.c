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
#include <stdbool.h>
#include "version.h"
#include "cl_lights/cl_comm_thread.h"
#include <assert.h>
#include "ws2811.h"
#include "log.h"

#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN_ONE            18
#define GPIO_PIN_TWO            13
#define DMA                     10
#define STRIP_TYPE              WS2811_STRIP_GRB		// WS2812/SK6812RGB integrated chip+leds

#define DEFAULT_LED_COUNT 5
#define DEFAULT_PI_NAME "Pi Name Unset"
#define DEFAULT_CONTROL_PI_PORT 15556
#define DEFAULT_CONTROL_PI_IP "127.0.0.1"
#define DEFAULT_STRIP_NUMBER 99
int32_t gpio_pin = -1;
uint32_t running = 1;
struct comm_thread_params comm_thread_params;
#define DEBUG_LOGGING
static void ctrl_c_handler(int signum)
{
    log_info("Control+C GET!");
	running = 0;
    (void)(signum);
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
        {"port", required_argument, 0, 'P'},
        {"ip", required_argument, 0, 'I'},
        {"length", required_argument, 0, 'L'},
        {"name", required_argument, 0, 'N'},
        {"gpio", required_argument, 0, 'G'},
        {"strip", required_argument, 0, 'S'},
        {"debug", optional_argument, 0, 'd'},
        {"help", optional_argument, 0, 'h'},
        {0, 0, 0, 0}
	};

	while (1)
	{

		index = 0;
		c = getopt_long(argc, argv, "hH:P:I:L:N:G:d:S:", longopts, &index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* handle flag options (array's 3rd field non-0) */
			break;
        case 'P':
            if (optarg) {
                comm_thread_params.control_pi_port = atoi(optarg);
            }
            break;
        case 'I':
            if (optarg) {
                comm_thread_params.control_pi_ip = (char*) malloc(strlen(optarg) + 1);
                strcpy(comm_thread_params.control_pi_ip, optarg);
            }
            break;
        case 'N':
            if (optarg) {
                comm_thread_params.pi_name_length = strlen(optarg) + 1;
                comm_thread_params.pi_name = (char*) malloc(comm_thread_params.pi_name_length);
                strcpy(comm_thread_params.pi_name, optarg);
            }
            break;
        case 'L':
            if (optarg) {
                comm_thread_params.led_array_length = atoi(optarg);
            }
            break;
        case 'G':
            if (optarg) {
                gpio_pin = atoi(optarg);
            }
            break;
        case 'S':
            if (optarg) {
                comm_thread_params.strip_number = atoi(optarg);
            }
            break;
        case 'd':
            log_set_level(LOG_TRACE);
            break;
        case 'H':
        case 'h':
            fprintf(stderr, "P / --port: Control Pi Port Number\n");
            fprintf(stderr, "I / --ip: Control Pi IP Address\n");
            fprintf(stderr, "N / --name: The name of this pi\n");
            fprintf(stderr, "L / --length: The number of LEDs on this strip\n");
            fprintf(stderr, "G / --gpio: Either the first GPIO pin or the second one\n");
            fprintf(stderr, "H / --help: This help menu\n");
            fprintf(stderr, "h / --help: This help menu\n");
            exit(0);
        }

    }
    if (gpio_pin != 1 && gpio_pin != 2) {
        log_fatal("Bad pin\n");
        exit (-1);
    }
}

ws2811_t *
create_ledstring(uint32_t gpio_pin, uint32_t led_count)
{
    ws2811_t *ledstring; 
    ledstring = (ws2811_t*)calloc(1, sizeof(ws2811_t));
    if (ledstring == NULL) {
        log_fatal("Out of Memory: Cannot allocate space for ledstring");
        return NULL;
    }

    ledstring->freq = TARGET_FREQ;
    ledstring->dmanum = DMA;
    if (gpio_pin == 1) {
        ledstring->channel[0].gpionum = GPIO_PIN_ONE;
        ledstring->channel[0].count = led_count;
        ledstring->channel[0].invert = 0;
        ledstring->channel[0].brightness = 100;
        ledstring->channel[0].strip_type = STRIP_TYPE;
    }
    else {
        ledstring->channel[1].gpionum = GPIO_PIN_TWO;
        ledstring->channel[1].count = led_count;
        ledstring->channel[1].invert = 0;
        ledstring->channel[1].brightness = 100;
        ledstring->channel[1].strip_type = STRIP_TYPE;
    }

    if (ws2811_init(ledstring) != WS2811_SUCCESS) {
        log_fatal("Unable to initialize LED string");
        free(ledstring);
        ledstring = NULL;
    }
    else {
        log_debug("LED String initialized");
    }
    return ledstring;
}

void
print_ledstring(ws2811_t *ledstring)
{
#ifdef DEBUG_LOGGING
    uint32_t x;
    printf("XXX LED String: [");
    uint32_t channel = ledstring->channel[0].gpionum == GPIO_PIN_ONE ? 0 : 1;
    uint32_t size = ledstring->channel[channel].count;
    for (x = 0; x < size; x++) {
        printf("%u, ", ledstring->channel[channel].leds[x]);
    }
    printf("]\n");
#else
    assert(ledstring != NULL);
#endif
}

static void
print_disp_buf(struct comm_thread_params *params) {
#ifdef DEBUG_LOGGING
    int i = 0;
    int len = params->led_array_length;
    printf("XXX: Display Buffer: [");
    while (i < len) {
        printf("%u, ", (*(params->displaying_array))[i]);
        i++;
    }
    printf("]\n");
#else
    assert(params != NULL);
#endif
}

static void
stamp_to_led(struct comm_thread_params *params, ws2811_t *ledstring)
{
    int i = 0;
    int len = params->led_array_length;
    uint32_t channel = ledstring->channel[0].gpionum == GPIO_PIN_ONE ? 0 : 1;

    while (i < len) {
        switch ( (*(params->displaying_array))[i]) {
            case 0:
                ledstring->channel[channel].leds[i] = 0x000000;
                break;
            case 1:
                ledstring->channel[channel].leds[i] = 0xff;
               break;
            case 2:
                ledstring->channel[channel].leds[i] = 0xff00;
                break;
            case 3:
                ledstring->channel[channel].leds[i] = 0xff0000;
                break;
            default:
                printf("WTF IS THIS? %d\n", (*(params->displaying_array))[i]);
                break;
        }
        i++;
    }
//    while (i < len) {
//        ledstring->channel[channel].leds[i] = (*(params->displaying_array))[i];
//        i++;
//    }
}

static void
change_displaying_array(struct comm_thread_params *params)
{
    if (*params->receiving_array == params->led_array_buf_1) {
        if (*params->displaying_array == params->led_array_buf_2) {
            log_trace("Changing display buffer from 1 to 3");
            params->displaying_array = &params->led_array_buf_3;
        }
        else {
            log_trace("Changing display buffer from 1 to 2");
            params->displaying_array = &params->led_array_buf_2;
        }
    }
    else if (*params->receiving_array == params->led_array_buf_2) {
        if (*params->displaying_array == params->led_array_buf_1) {
            log_trace("Changing display buffer from 2 to 3");
            params->displaying_array = &params->led_array_buf_3;
        }
        else {
            log_trace("Changing display buffer from 2 to 1");
            params->displaying_array = &params->led_array_buf_1;
        }
    }
    else if (*params->receiving_array == params->led_array_buf_3) {
        if (*params->displaying_array == params->led_array_buf_1) {
            log_trace("Changing display buffer from 3 to 2");
            params->displaying_array = &params->led_array_buf_2;
        }
        else {
            log_trace("Changing display buffer from 3 to 1");
            params->displaying_array = &params->led_array_buf_1;
        }
    }
    else {
        log_error("Unable to change display buffer");
        running = 0;
    }

}

int main(int argc, char *argv[])
{
    log_set_level(LOG_FATAL);
    
    pthread_mutex_t recv_disp_ptr_lock = PTHREAD_MUTEX_INITIALIZER;

    comm_thread_params.control_pi_ip       = (char*) calloc(strlen(DEFAULT_CONTROL_PI_IP) + 1, sizeof(char));
    strcpy(comm_thread_params.control_pi_ip, DEFAULT_CONTROL_PI_IP);
    comm_thread_params.control_pi_port     = DEFAULT_CONTROL_PI_PORT;
    comm_thread_params.pi_name_length      = strlen(DEFAULT_PI_NAME) + 1;
    comm_thread_params.pi_name             = (char*) calloc(comm_thread_params.pi_name_length, sizeof(char));
    strcpy(comm_thread_params.pi_name, DEFAULT_PI_NAME);
    comm_thread_params.led_array_length    = DEFAULT_LED_COUNT;
    comm_thread_params.strip_number        = (uint32_t) DEFAULT_STRIP_NUMBER;
    parseargs(argc, argv);

    comm_thread_params.led_array_buf_1     = (uint32_t*) calloc(comm_thread_params.led_array_length, sizeof(uint32_t));
    comm_thread_params.led_array_buf_2     = (uint32_t*) calloc(comm_thread_params.led_array_length, sizeof(uint32_t));
    comm_thread_params.led_array_buf_3     = (uint32_t*) calloc(comm_thread_params.led_array_length, sizeof(uint32_t));

    // XXX: How do I make this a one liner for each variable?
    uint32_t *receiving_array = comm_thread_params.led_array_buf_1;
    uint32_t *displaying_array = comm_thread_params.led_array_buf_2;
    comm_thread_params.receiving_array = &receiving_array;
    comm_thread_params.displaying_array = &displaying_array;
    comm_thread_params.recv_disp_ptr_lock  = &recv_disp_ptr_lock;
    comm_thread_params.new_data            = false;

    /* Handlers should only be caught in this file. And commands propogate down */
    setup_handlers();

    log_info("Activating network communicator");
    run_net_comm(&comm_thread_params);

    /* receiving_array - The array being written to
     * displaying_array - The array I am displaying OR just finished displaying
     * new_data - If true, there is new data. To determine which buffer has the new
     *            data, it is the buffer that is not pointed to from receiving_array
     *            or displaying array.
     *  Example: I just finished displaying two, and set new_data to false. Something new has
     *  been inserted into one and receiving_data is now pointed at three, and new_data is set
     *  back to true. I see that new data is true,
     *  that receiving_array points at three and displaying_array still points at two.
     *  Thus, I change displaying_array to one, set new_data to false,
     *  and start displaying what is in displaying_array / what is in led_array_buf_1
     */

    ws2811_t *ledstring = create_ledstring(gpio_pin, comm_thread_params.led_array_length);
    if (ledstring == NULL) {
        exit(-1);
    }

    log_info("Pi Controller is now running");
    
    while(running == 1) {
        log_trace("XXX: display waiting\n");
        if (comm_thread_params.new_data) {
            comm_thread_params.new_data = false;
            pthread_mutex_lock(comm_thread_params.recv_disp_ptr_lock);
            print_disp_buf(&comm_thread_params);
    
            stamp_to_led(&comm_thread_params, ledstring);
            change_displaying_array(&comm_thread_params);
            pthread_mutex_unlock(comm_thread_params.recv_disp_ptr_lock);

            if (ws2811_render(ledstring) != WS2811_SUCCESS) {
                log_error("ws2811_render failed: ");
                exit(-1);
            }
            print_ledstring(ledstring);


        }
        sleep(2);
    }
    log_info("Pi Controller is now stopping");
    end_net_comm(&comm_thread_params);

    free(comm_thread_params.pi_name);
    free(comm_thread_params.control_pi_ip);
    //free(comm_thread_params.led_array_buf_1);
    //free(comm_thread_params.led_array_buf_2);
    //free(comm_thread_params.led_array_buf_3);
    ws2811_fini(ledstring);
    free(ledstring);
    log_info("Pi Controller is now stopped");
    return 0;
}
