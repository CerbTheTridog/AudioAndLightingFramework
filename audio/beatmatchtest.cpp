#include "beatmatch.h"
#include "beatmatchevent.h"
#include "lib/libfft.h"
#include <signal.h>
#include <stdio.h>

static bool running = true;
BeatMatchEvent *bme;
//pthread_mutex_t callBackMutex = PTHREAD_MUTEX_INITIALIZER;

int main()
{
	void signalHandler( int signum );
        void floatGet (float *input, int size);
	struct sigaction action;
	
	action.sa_handler = signalHandler;
	sigemptyset (&action.sa_mask);
	action.sa_flags = 0;

	sigaction (SIGINT, &action, NULL);
	sigaction (SIGHUP, &action, NULL);
	sigaction (SIGTERM, &action, NULL);
	
	bme = new BeatMatchEvent(48000, 8192, 13);
	
	bme->StartThread(floatGet);
	
	while( running ){ }
	
	return 0;
}

void signalHandler( int signum ) { 
	running = false; 
	bme->StopThread();
}

void floatGet (float *input, int size) {
    //pthread_mutex_lock( &callBackMutex );
    for( int j=0; j < size; ++j ) {
        printf("From floatGet: %g\n", input[j]);
    }
    //pthread_mutex_unlock( &callBackMutex );
}