#include "beatmatch.h"
#include "beatmatchevent.h"
#include "audio/lib/libfft.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include "ws2811.h"
#include "pattern_pulse.h"
#include "log.h"

static bool running = true;
BeatMatchEvent *bme;

//LED String Info
/*
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
#define STRIP_TYPE              WS2811_STRIP_GRB		// WS2812/SK6812RGB
*/
#define LED_COUNT				300

static struct pattern *pattern;

int main()
{
	void signalHandler( int signum );
    void floatGet (float *input, int size);
	void colorGet(int color, int intensity); 
	struct sigaction action;
	
	action.sa_handler = signalHandler;
	sigemptyset (&action.sa_mask);
	action.sa_flags = 0;

	sigaction (SIGINT, &action, NULL);
	sigaction (SIGHUP, &action, NULL);
	sigaction (SIGTERM, &action, NULL);
	
	log_set_level(LOG_TRACE);
	
	pattern = create_pattern();
	ws2811_return_t ret;
	
    if ((ret = configure_ledstring_single(pattern, LED_COUNT)) != WS2811_SUCCESS) {
            log_fatal("Bad stuff");
            return ret;
    }

	if ((ret = pulse_create(pattern)) != WS2811_SUCCESS) {
		log_fatal("pulse_create failed: %s", ws2811_get_return_t_str(ret));
		return ret;
	}
   
    pattern->clear_on_exit = true;
    pattern->movement_rate = 100;
    pattern->pulseWidth = 0;

    /* Load the program into memory */
    pattern->func_load_pattern(pattern);

    /* Start the program */
    pattern->func_start_pattern(pattern);
	
	bme = new BeatMatchEvent(48000, 8192, 13, 440, 8, true, NULL, colorGet);
	
	bme->StartThread();
	
	while( running ){ }
	
	return 0;
}

void signalHandler( int signum ) { 
    printf("%d\n", signum);
    running = false; 
	bme->StopThread();
	pattern->func_kill_pattern(pattern);
}

//Callback that receives the array of observed frequencies and their loudness.
//Use this if you want to trigger some custom behavior.
void floatGet (float *input, int size) {
    for( int j=0; j < size; ++j ) {
        printf("From floatGet: %g\n", input[j]);
    }
}

//Callback that received "Campcon" style lighting and intensity.
void colorGet(int color, int intensity) {
	printf("%d color : %d intensity\n", color, intensity);
	if(pattern)
		pattern->func_inject(color, intensity);
}
