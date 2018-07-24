#include "beatmatch.h"
#include "beatmatchevent.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

BeatMatchEvent::BeatMatchEvent(int sample_rate, int fft_size, int fft_exp, int beat_hz, int beat_window_size, bool ignore_repeat_beat, void(*freqCb)(float*, int), void(*cb)(int, int)) : BeatMatch(sample_rate, fft_size, fft_exp)
{ 
	BeatHz = beat_hz; 
	FreqCallback = freqCb;
	Callback = cb;
	IgnoreRepeatBeat = ignore_repeat_beat;
	BeatWindowSize = beat_window_size;
	BeatWindow = (float*)malloc(sizeof(float) * beat_window_size);
	Beats = (int *)malloc(sizeof(int) * beat_window_size);
	ActualBeats = (int *)malloc(sizeof(int) * beat_window_size);
	
	for(int i = 0; i < beat_window_size; i++) {
		BeatWindow[i] = 0.0f;
		Beats[i] = 0;
		ActualBeats[i] = 0;
	}
	
	//NoteIndexes = (float*)malloc(sizeof(float) * beat_window_size * fft_size / 2 * 2);
	NoteIndexes = (float ***)malloc(beat_window_size * sizeof(float **));
	for(int i = 0; i < beat_window_size; i++) 
	{
		NoteIndexes[i] = (float **)malloc(fft_size / 2 * sizeof(float *));
		for(int j = 0; j < fft_size / 2; j++) 
		{
			NoteIndexes[i][j] = (float *)malloc(sizeof(float) * 2);
		}
	}
}

void BeatMatchEvent::EventThread()
{
	float transform = 0;
	float red_transform = 0;
	float blue_transform = 0;
	float green_transform = 0;
	float color_slope = (float)255/(float)6;
	
	uint32_t color = 0;
	int intensity = 0;
	float avgFreq = 0.0f;
	float avgMag = 0.0f;
	int avgMagCount = 0;
	int avgFreqCount = 0;
	
    while (running)
    {
        err = Pa_ReadStream( stream, data, FFTSize );
        if( err && err != -9981 )
        {
            throw "Pa_ReadStream threw error " + err;
        }

        for( int j=0; j < FFTSize; ++j ) {
            data[j] = processSecondOrderFilter( data[j], mem1);
            data[j] = processSecondOrderFilter( data[j], mem2);
        }

        applyWindow( data );

        for( int j=0; j < FFTSize; ++j )
            datai[j] = 0;
        applyfft( fft, data, datai, false );

		float maxVal = -1;
		float minVal = SampleRate;
		float avgVal = 0;
		int maxIndex = -1;
        float *frame = (float*)malloc(sizeof(float) * FFTSize / 2);
        for( int j=0; j < FFTSize / 2; ++j ) {
            frame[j] = data[j] * data[j] + datai[j] * datai[j];
			
			if( frame[j] > maxVal ) {
				maxVal = frame[j];
				maxIndex = j;
			}
			 
			if( frame[j] < minVal) {
				minVal = frame[j];
			}
			 
			if((j * SampleRate / FFTSize) < BeatHz)
				avgVal += frame[j];
        }
		
        //Call out raw frequency callback in case they want to do anything.
		if(FreqCallback)
			FreqCallback(frame, FFTSize / 2);
        
		avgVal = avgVal / FFTSize / 2;
		
		for(int i = BeatWindowSize - 1; i > 0; i--) {
			BeatWindow[i] = BeatWindow[i - 1];
			Beats[i] = Beats[i - 1];
			ActualBeats[i] = ActualBeats[i - 1];

			//memcpy(NoteIndexes[i], &NoteIndexes[i - 1], sizeof(float) * (FFTSize / 2) * 2);
			for(int j = 0; j < FFTSize / 2; j++) 
			{
				//memcpy(NoteIndexes[i][j], &NoteIndexes[i - 1][j], sizeof(float) * 2);
				NoteIndexes[i][j][0] = NoteIndexes[i - 1][j][0];
				NoteIndexes[i][j][1] = NoteIndexes[i - 1][j][1];
			}
		}
		
		for( int j = 0; j < FFTSize / 2; ++j ) {
			NoteIndexes[0][j][0] = 0.0f;
			NoteIndexes[0][j][1] = 0.0f;
		}
		
		for( int j = 0; j < FFTSize / 2; ++j ) {
			if(j > 1 && j < FFTSize / 2 - 2) {
				if(
					frame[j] > frame[j-1] && 
					frame[j] > frame[j-2] && 
					frame[j] > frame[j+1] && 
					frame[j] > frame[j+2]
				) {
					NoteIndexes[0][j][0] = ( SampleRate * j ) / (float) ( FFTSize );
					NoteIndexes[0][j][1] = frame[j];
				}
			 }
		}
		
		int beat = 0; 
		int higherAvg = 0;
		
		for(int i = 0; i < BeatWindowSize; i++) {
			if(avgVal > BeatWindow[i]) higherAvg++;
		}
		
		if(higherAvg > BeatWindowSize / 2 && (avgVal * 1000) > 0.00001) {
			beat = 1;
		}
		
		BeatWindow[0] = avgVal;
		Beats[0] = beat;
		
		if(Beats[1] != 0 && IgnoreRepeatBeat == true)
			beat = 0;
		
		ActualBeats[0] = beat;
		
		if(beat == 1) {
			avgFreq = 0.0f;
			avgMag = 0.0f;
			avgMagCount = 0;
			avgFreqCount = 0;
			 
			for(int i = 0; i < BeatWindowSize; i++) {
				if(i > 0 && ActualBeats[i] == 1) {
					break;
				}
				
				for(int j = 1; j < FFTSize / 2; j++) {
					if(NoteIndexes[i][j][0] > 0 && Decibel(NoteIndexes[i][j][1]) > -120) {
						if(i == 0) {
							avgMag += NoteIndexes[i][j][1];
							avgMagCount++;
							if(color == 0) {
								avgFreq += NoteIndexes[i][j][0];
								avgFreqCount++;
							}
						} else {
							avgFreq += NoteIndexes[i][j][0];
							avgFreqCount++;
						}
					}
				}
			}
			
			if(avgMagCount != 0) {
				avgMag = avgMag / avgMagCount;
				
				if(Decibel(avgMag) > -40) {
					intensity = 100;
				}
				else 
				{
					intensity = (int)((Decibel(avgMag)) + 120) * 1.25f;
				}
			}
			
			if(avgFreqCount != 0) {
				avgFreq = avgFreq / avgFreqCount;
			
				transform = (float)avgFreq * (float)FFTSize / (float)SampleRate;
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
				//printf("R:%2.2f G:%2.2f B:%2.2f transform:%2.2f Final Color: \n", red_transform, green_transform, blue_transform, transform, myColor);
				color = myColor;
			}
			
			if(Callback)
				Callback(color, intensity);
		}
		

		free(frame);
    }
}

float BeatMatchEvent::Decibel(float in) 
{
	return 20*log10(in);
}