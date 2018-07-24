#include <pthread.h>
#include "lib/portaudio.h"

class BeatMatch 
{
        pthread_t thread_id;
        static void *InternalThread(void*);
		float a[2], b[3];
    public:
        BeatMatch(int, int, int);
        virtual ~BeatMatch() {};
        bool StartThread();
        bool StopThread();
    protected:
        void buildHanWindow(  );
        void applyWindow( float* );
        void computeSecondOrderLowPassParameters( float, float );
        float processSecondOrderFilter( float, float* );
		//Implement this method to do whatever you want with the audio info.
        virtual void EventThread() = 0;
        bool running;
        PaStream *stream;
        PaError err;
        PaStreamParameters inputParameters;
        const PaDeviceInfo *deviceInfo;
        int SampleRate;
        int FFTSize;
        int FFTExp;
        float *data;
        float *datai;
        void *fft;
        float *window;
		float mem1[4], mem2[4];
};