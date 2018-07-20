/* main.c - chromatic guitar tuner
 *
 * Copyright (C) 2012 by Bjorn Roche
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  This software is provided "as is" without express or
 * implied warranty.
 *
 */

 //Yeah I GEHSTOLEN this code to cannibalize for my beat detector/light emitter.
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include "libfft.h"
#include <portaudio.h>

#include <stdint.h>

#include "lights/rpi_ws281x/ws2811.h"
#include "lights/pattern_pulse.h"
#include "lights/log.h"

/* -- some basic parameters -- */
#define SAMPLE_RATE (48000)
#define FFT_SIZE (8192)
#define FFT_EXP_SIZE (13)
#define BEAT_HZ (440)
#define IGNORE_REPEAT_BEAT (1)
#define COLOR_SCALE (95869)


#define BEAT_WINDOW (8)

//LED String Info
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
#define STRIP_TYPE              WS2811_STRIP_GRB		// WS2812/SK6812RGB
#define LED_COUNT		300

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 100,
            .strip_type = STRIP_TYPE,
        },
        [1] =
        {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
            .brightness = 0,
        },
    },
};

/* -- functions declared and used here -- */
void buildHanWindow( float *window, int size );
void applyWindow( float *window, float *data, int size );
//a must be of length 2, and b must be of length 3
void computeSecondOrderLowPassParameters( float srate, float f, float *a, float *b );
//mem must be of length 4.
float processSecondOrderFilter( float x, float *mem, float *a, float *b );
void signalHandler( int signum ) ;

static bool running = true;

static struct pattern *pattern;

//FILE *fle;

/* -- main function -- */
int main( int argc, char **argv ) {
   PaStreamParameters inputParameters;
   float a[2], b[3], mem1[4], mem2[4];
   float data[FFT_SIZE];
   float datai[FFT_SIZE];
   float window[FFT_SIZE];
   float freqTable[FFT_SIZE];
   float noteIndexes[BEAT_WINDOW][FFT_SIZE/2][2];
   float beatWindow[BEAT_WINDOW];
   int beats[BEAT_WINDOW];
   int actualBeats[BEAT_WINDOW];
   //char * noteNameTable[FFT_SIZE];
   //float notePitchTable[FFT_SIZE];
   float transform = 0;
   float red_transform = 0;
   float blue_transform = 0;
   float green_transform = 0;
   float color_slope = (float)255/(float)6;
   
   void * fft = NULL;
   PaStream *stream = NULL;
   PaError err = 0;
   struct sigaction action;

   //BWP
   	const PaDeviceInfo *deviceInfo;
	int numDevices, deviceIndex = -1;
  
   /*fle = fopen("/home/pi/samples7.txt", "a");
   if (fle == NULL){
     printf("OH FUCK\n");
     exit(1);
   }*/

   // add signal listen so we know when to exit:
   action.sa_handler = signalHandler;
   sigemptyset (&action.sa_mask);
   action.sa_flags = 0;

   sigaction (SIGINT, &action, NULL);
   sigaction (SIGHUP, &action, NULL);
   sigaction (SIGTERM, &action, NULL);

  //LED Initialize
  log_set_level(LOG_TRACE);
	ws2811_return_t ret;
	
    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        log_fatal("ws2811_init failed: %s", ws2811_get_return_t_str(ret));
        return ret;
    }
   
	if ((ret = pulse_create(&pattern)) != WS2811_SUCCESS) {
		log_fatal("pulse_create failed: %s", ws2811_get_return_t_str(ret));
		return ret;
	}
   
       /* Configure settings */
    pattern->width = LED_COUNT;
    pattern->height = 1;
    pattern->led_count = LED_COUNT;
    pattern->clear_on_exit = true;
    pattern->ledstring = ledstring;
    //pattern->maintainColor = maintain_colors;
    pattern->movement_rate = 100;
    pattern->pulseWidth = 0;
    /* Load the program into memory */
    pattern->func_load_pattern(pattern);

    /* Start the program */
    pattern->func_start_pattern(pattern);
   
   //END LED
   
   for(int i = 0; i < BEAT_WINDOW; i++) {
	beatWindow[i] = 0.0f;
	beats[i] = 0;
	actualBeats[i] = 0;
   }
   
   // build the window, fft, etc
   buildHanWindow( window, FFT_SIZE );
   fft = initfft( FFT_EXP_SIZE );
   computeSecondOrderLowPassParameters( SAMPLE_RATE, 330, a, b );
   mem1[0] = 0; mem1[1] = 0; mem1[2] = 0; mem1[3] = 0;
   mem2[0] = 0; mem2[1] = 0; mem2[2] = 0; mem2[3] = 0;
   //freq/note tables
   for( int i=0; i<FFT_SIZE; ++i ) {
      freqTable[i] = ( SAMPLE_RATE * i ) / (float) ( FFT_SIZE );
   }

   // initialize portaudio
   err = Pa_Initialize();
   if( err != paNoError ) goto error;

   	numDevices = Pa_GetDeviceCount();
	if( numDevices < 0 ) {
		printf( "ERROR: Pa_GetDeviceCount returned 0x%x\n", numDevices );
		err = numDevices;
		pattern->func_kill_pattern(pattern);
		return err;
	}
   
   	for(int i = 0; i < numDevices; i++) {
		deviceInfo = Pa_GetDeviceInfo(i);
		//printf( "Index: %d    Name: %s\n", i, deviceInfo->name );
		if(strstr(deviceInfo->name, "USB") != NULL) {
			deviceIndex = i;
			break;
		}
	}
	
	if(deviceIndex < 0) {
		printf("USB Audio Not Found\n");
		pattern->func_kill_pattern(pattern);
		return 1;
	}
   
   
   inputParameters.device = deviceIndex;
   inputParameters.channelCount = 1;
   inputParameters.sampleFormat = paFloat32;
   inputParameters.suggestedLatency = deviceInfo->defaultHighInputLatency ;
   inputParameters.hostApiSpecificStreamInfo = NULL;

   printf( "Opening %s\n",
           Pa_GetDeviceInfo( inputParameters.device )->name );

   err = Pa_OpenStream( &stream,
                        &inputParameters,
                        NULL, //no output
                        SAMPLE_RATE,
                        FFT_SIZE,
                        paClipOff,
                        NULL,
                        NULL );
   if( err != paNoError ) goto error;

   err = Pa_StartStream( stream );
   if( err != paNoError ) goto error;

   // this is the main loop where we listen to and
   // process audio.
	
	int priorBeat = 0;
	
	uint32_t color = 0;
	int intensity = 0;
	float avgFreq = 0.0f;
	float avgMag = 0.0f;
	int avgMagCount = 0;
	int avgFreqCount = 0;
	
   while( running )
   {
      // read some data
      err = Pa_ReadStream( stream, data, FFT_SIZE );
      if( err && err != -9981 ) goto error; //FIXME: we don't want to err on xrun
  
      for( int j=0; j<FFT_SIZE; ++j ) {
         data[j] = processSecondOrderFilter( data[j], mem1, a, b );
         data[j] = processSecondOrderFilter( data[j], mem2, a, b );
      }

      // window
      applyWindow( window, data, FFT_SIZE );

      // do the fft
      for( int j=0; j<FFT_SIZE; ++j )
         datai[j] = 0;
      applyfft( fft, data, datai, false );

      //find the peak
      float maxVal = -1;
	  float minVal = SAMPLE_RATE;
	  float avgVal = 0;
      int maxIndex = -1;
	  
      for( int j=0; j<FFT_SIZE/2; ++j ) {
         float v = data[j] * data[j] + datai[j] * datai[j] ;

         if( v > maxVal ) {
            maxVal = v;
            maxIndex = j;
         }
		 
		 if( v < minVal) {
			minVal = v;
		 }
		 
		 if((j * SAMPLE_RATE / FFT_SIZE) < BEAT_HZ)
			avgVal += v;
      }
	  
	  avgVal = avgVal / FFT_SIZE / 2;
	 
	for(int i = BEAT_WINDOW - 1; i > 0; i--) {
		beatWindow[i] = beatWindow[i - 1];
		beats[i] = beats[i - 1];
		actualBeats[i] = actualBeats[i - 1];
		//noteIndexes[i] = noteIndexes[i - 1];
		memcpy(noteIndexes[i], &noteIndexes[i - 1], sizeof(float) * (FFT_SIZE / 2) * 2);
	}
	
	for( int j=0; j<FFT_SIZE/2; ++j ) {
		noteIndexes[0][j][0] = 0.0f;
		noteIndexes[0][j][1] = 0.0f;
	}
	
	for( int j=0; j<FFT_SIZE/2; ++j ) {
		float v = data[j] * data[j] + datai[j] * datai[j] ;
		if(j > 1 && j < FFT_SIZE / 2 - 2) {
			if(
				v > (data[j-1] * data[j-1] + datai[j-1] * datai[j-1]) && 
				v > (data[j-2] * data[j-2] + datai[j-2] * datai[j-2]) && 
				v > (data[j+1] * data[j+1] + datai[j+1] * datai[j+1]) && 
				v > (data[j+2] * data[j+2] + datai[j+2] * datai[j+2]) 
			) {
				noteIndexes[0][j][0] = freqTable[j];
				noteIndexes[0][j][1] = v;
			}
		 }
	}
	
	int beat = 0; 
	
	int higherAvg = 0;
	
	for(int i = 0; i < BEAT_WINDOW; i++) {
		if(avgVal > beatWindow[i]) higherAvg++;
	}
	
	if(higherAvg > BEAT_WINDOW / 2 && (avgVal * 1000) > 0.00001) {
		beat = 1;
	}
	
	beatWindow[0] = avgVal;
	beats[0] = beat;
	
	if(beats[1] != 0 && IGNORE_REPEAT_BEAT == 1)
		beat = 0;
	
	actualBeats[0] = beat;
	
	float freq = freqTable[maxIndex];
	
	if(beat == 1) {
		avgFreq = 0.0f;
		avgMag = 0.0f;
		avgMagCount = 0;
		avgFreqCount = 0;
		 
		for(int i = 0; i < BEAT_WINDOW; i++) {
			if(i > 0 && actualBeats[i] == 1)
				break;
			for(int j = 1; j < FFT_SIZE/2; j++) {
				if(noteIndexes[i][j][0] > 0 && 20*log10(noteIndexes[i][j][1]) > -120) {
					if(i == 0) {
						avgMag += noteIndexes[i][j][1];
						avgMagCount++;
						if(color == 0) {
							avgFreq += noteIndexes[i][j][0];
							avgFreqCount++;
						}
					} else {
						avgFreq += noteIndexes[i][j][0];
						avgFreqCount++;
					}
				}
			}
		}
		
		if(avgMagCount != 0) {
			avgMag = avgMag / avgMagCount;
			
			if(20*log10(avgMag) > -40) {
				intensity = 100;
			}
			else 
			{
				intensity = (int)((20*log10(avgMag)) + 120) * 1.25f;
			}
		}
		
		if(avgFreqCount != 0) {
			avgFreq = avgFreq / avgFreqCount;
		
			/*if(avgFreq > 1025.390625)
			{
				color = 16777215;
			}
			else 
			{*/
				//color = (int)avgFreq * FFT_SIZE / SAMPLE_RATE;
				transform = (float)avgFreq * (float)FFT_SIZE / (float)SAMPLE_RATE;
				//cut 30 and 70
				if (transform < 30 ) {
				   transform = 30;
				}
				else if (transform > 70) {
				  transform = 70;
				}
				//flip 41 or 40
				//flip 60 or 59
				if (transform < 41) {
				   transform = 41 + (41-transform);
				} else if (transform > 58) {
				   transform = 58 - (transform-58);
				}
				//fprintf(fle, "%g\n", transform);
				//color *= COLOR_SCALE;

				//map to color
				//41-47 red 255->0 green 0->255
				if(transform <=47) {
				   red_transform = 255 - ((47-transform) * color_slope);
				   green_transform = (47-transform) * color_slope;
				   blue_transform = 0;
                                 }
				//47-53 green 255->0 blue 0->255
				if(transform >47 && transform <=53) {
				   green_transform = 255 - ((53-transform) * color_slope);
				   blue_transform = (53-transform) * color_slope;
				   red_transform = 0;
				}
				//53-59 blue 255->0 red 0->255
				if (transform >53) {
				   blue_transform = 255 - ((59-transform) * color_slope);
				   red_transform = (59-transform) * color_slope;
				   green_transform = 0;
				}

				uint32_t myColor = (uint32_t)blue_transform + ((uint32_t)green_transform << 8) + ((uint32_t)red_transform << 16);
				//fprintf(fle, "R:%2.2f G:%2.2f B:%2.2f transform:%2.2f Final Color: \n", red_transform, green_transform, blue_transform, transform, myColor);
				color = myColor;
				
			//}
		}
		printf("FUCK\n");
		pattern->func_inject(color, intensity);
	}
	  
      // now output the results:
      printf("\033[2J\033[1;1H"); //clear screen, go to top left
      fflush(stdout);

      printf( "Tuner listening. Control-C to exit.\n" );
      printf( "%09.5f Hz, %d : %03.5f/%03.5f : %03.5f Beat: %d\n", freq, maxIndex, 20*log10(minVal), 20*log10(maxVal), 20*log10(avgVal), beat );
	  printf( "Avg History:\n" );
	  for(int i = 0; i < BEAT_WINDOW; i++) {
		printf("%03.5f\n", 20*log10(beatWindow[i]));
	  }
	  printf("\n");
	  printf( "Actual Beats: " );
	  for(int i = 0; i < BEAT_WINDOW; i++) {
		printf("%d ", actualBeats[i]);
	  }
	  printf( "\n" );
	  printf( "Played Beats: " );
	  for(int i = 0; i < BEAT_WINDOW; i++) {
		printf("%d ", beats[i]);
	  }
	  printf("\n");
	  printf("Avg Freq: %09.5f Hz\n", avgFreq);
	  printf("Color: %x\n", color);
	  printf("Avg Amp: %03.5f\n", 20*log10(avgMag));
	  printf("Intensity: %d\n", intensity);
	  /*printf("\n");
	  printf("Peaks at:\n");
	  for(int j = 1; j < FFT_SIZE/2; j++) {
		  if(noteIndexes[j][0] > 0 && 20*log10(noteIndexes[j][1]) > -120)
			printf("%09.5f : %03.5f\n", noteIndexes[j][0], 20*log10(noteIndexes[j][1]));
	  }*/
   }
   err = Pa_StopStream( stream );
   if( err != paNoError ) goto error;

   // cleanup
   destroyfft( fft );
   Pa_Terminate();

   return 0;
 error:
   if( stream ) {
      Pa_AbortStream( stream );
      Pa_CloseStream( stream );
   }
   destroyfft( fft );
   Pa_Terminate();
   fprintf( stderr, "An error occured while using the portaudio stream\n" );
   fprintf( stderr, "Error number: %d\n", err );
   fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
   return 1;
}

void buildHanWindow( float *window, int size )
{
   for( int i=0; i<size; ++i )
      window[i] = .5 * ( 1 - cos( 2 * M_PI * i / (size-1.0) ) );
}
void applyWindow( float *window, float *data, int size )
{
   for( int i=0; i<size; ++i )
      data[i] *= window[i] ;
}
void computeSecondOrderLowPassParameters( float srate, float f, float *a, float *b )
{
   float a0;
   float w0 = 2 * M_PI * f/srate;
   float cosw0 = cos(w0);
   float sinw0 = sin(w0);
   //float alpha = sinw0/2;
   float alpha = sinw0/2 * sqrt(2);

   a0   = 1 + alpha;
   a[0] = (-2*cosw0) / a0;
   a[1] = (1 - alpha) / a0;
   b[0] = ((1-cosw0)/2) / a0;
   b[1] = ( 1-cosw0) / a0;
   b[2] = b[0];
}
float processSecondOrderFilter( float x, float *mem, float *a, float *b )
{
    float ret = b[0] * x + b[1] * mem[0] + b[2] * mem[1]
                         - a[0] * mem[2] - a[1] * mem[3] ;

		mem[1] = mem[0];
		mem[0] = x;
		mem[3] = mem[2];
		mem[2] = ret;

		return ret;
}
void signalHandler( int signum ) { 
	running = false; 
	pattern->func_kill_pattern(pattern);
	//fclose(fle);
}
