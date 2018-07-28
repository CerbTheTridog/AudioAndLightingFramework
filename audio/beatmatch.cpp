#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include "lib/libfft.h"
#include "lib/portaudio.h"
#include "beatmatch.h"

BeatMatch::BeatMatch(int sample_rate, int fft_size, int fft_exp) 
{
    if((fft_size & (fft_size - 1)) != 0) 
    {
            throw "FFT Size must be a power of 2.";
    }

    SampleRate = sample_rate;
    FFTSize = fft_size;
    FFTExp = fft_exp;

    data = (float*)malloc(sizeof(float) * fft_size);
    if(data == NULL) 
    {
            throw "Could not allocate memory for data array.";
    }

    datai = (float*)malloc(sizeof(float) * fft_size);
    if(datai == NULL) 
    {
            throw "Could not allocate memory for datai array.";
    }

    window = (float*)malloc(sizeof(float) * fft_size);
    if(window == NULL) 
    {
            throw "Could not allocate memory for window.";
    }

    /*freqTable = (float*)malloc(sizeof(float) * fft_size);
    if(freqTable == NULL) 
    {
            throw "Could not allocate memory for freqTable.";
    }*/

    buildHanWindow( );
    fft = initfft( fft_exp );
    computeSecondOrderLowPassParameters( sample_rate, 330 );
    mem1[0] = 0; mem1[1] = 0; mem1[2] = 0; mem1[3] = 0;
    mem2[0] = 0; mem2[1] = 0; mem2[2] = 0; mem2[3] = 0;

    /*for( int i=0; i < fft_size; ++i ) {
        freqTable[i] = ( sample_rate * i ) / (float) ( fft_size );
    }*/
    
    running = false;
}

bool BeatMatch::StartThread() 
{
    int ret = 0;

    err = Pa_Initialize();
    if( err != paNoError )
    {
        throw "Pa_Initialize threw error " + err;
    }
    
    int numDevices = Pa_GetDeviceCount();
    if( numDevices < 0 ) {
        throw "ERROR: Pa_GetDeviceCount returned " + numDevices;
    }

    int deviceIndex = -1;
    for(int i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        if(strstr(deviceInfo->name, "USB") != NULL) {
            deviceIndex = i;
            break;
        }
    }

    if(deviceIndex < 0) {
        throw "USB Audio Not Found\n";
    }

    inputParameters.device = deviceIndex;
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = deviceInfo->defaultHighInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream( 
        &stream,
        &inputParameters,
        NULL,
        SampleRate,
        FFTSize,
        paClipOff,
        NULL,
        NULL );
    if( err != paNoError )
    {
        throw "Pa_OpenStream threw error " + err;
    }
    
    err = Pa_StartStream( stream );
    if( err != paNoError )
    {
        throw "Pa_StartStream threw error " + err;
    }
      
    running = true;

    ret = pthread_create(&thread_id, NULL, InternalThread, this);

    if(ret == 0)
    {
        return true;
    }
    else
    {
        throw "Could not create Thread, error code " + ret;
    }
}

bool BeatMatch::StopThread()
{
    running = false;
    pthread_join(thread_id, NULL);
    return true;
}

void *BeatMatch::InternalThread(void *This)
{
	//Implement this baby in a derived class.
    ((BeatMatch *)This)->EventThread(); 
    return NULL;
}

//Copyright (C) 2012 by Bjorn Roche
void BeatMatch::buildHanWindow( )
{
   for( int i=0; i < FFTSize; ++i )
      window[i] = .5 * ( 1 - cos( 2 * M_PI * i / (FFTSize-1.0) ) );
}

void BeatMatch::applyWindow( float *data )
{
   for( int i = 0; i < FFTSize; ++i )
      data[i] *= window[i] ;
}

void BeatMatch::computeSecondOrderLowPassParameters( float srate, float f )
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

float BeatMatch::processSecondOrderFilter( float x, float *mem )
{
    float ret = b[0] * x + b[1] * mem[0] + b[2] * mem[1]
                         - a[0] * mem[2] - a[1] * mem[3] ;

    mem[1] = mem[0];
    mem[0] = x;
    mem[3] = mem[2];
    mem[2] = ret;

    return ret;
}
