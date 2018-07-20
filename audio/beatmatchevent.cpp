#include "beatmatch.h"
#include "beatmatchevent.h"
#include <stdio.h>
#include <stdlib.h>

BeatMatchEvent::BeatMatchEvent(int sample_rate, int fft_size, int fft_exp) : BeatMatch(sample_rate, fft_size, fft_exp){ }

void BeatMatchEvent::EventThread()
{
    while (running)
    {
        //pthread_mutex_lock( &callBackMutex );
        err = Pa_ReadStream( stream, data, FFTSize );
        if( err && err != -9981 )
        {
            throw "Pa_ReadStream threw error " + err;
        }

        for( int j=0; j < FFTSize; ++j ) {
            data[j] = processSecondOrderFilter( data[j], mem1, a, b );
            data[j] = processSecondOrderFilter( data[j], mem2, a, b );
        }

        applyWindow( window, data, FFTSize );

        for( int j=0; j < FFTSize; ++j )
            datai[j] = 0;
        applyfft( fft, data, datai, false );

        float *frame = (float*)malloc(sizeof(float) * FFTSize / 2);
        for( int j=0; j < FFTSize / 2; ++j ) {
            frame[j] = data[j] * data[j] + datai[j] * datai[j];
        }
        //pthread_mutex_unlock( &callBackMutex );
        FreqCallback(frame, FFTSize / 2);
        free(frame);
    }
}