#include <pthread.h>
#include <portaudio.h>

class BeatMatch 
{
        //float *freqTable;
        pthread_t thread_id;
        static void *InternalThread(void*);
    public:
        BeatMatch(int, int, int);
        virtual ~BeatMatch() {};
        bool StartThread(void (*)(float *, int));
        bool StopThread();
    protected:
        //pthread_mutex_t callBackMutex;
        void buildHanWindow( float*, int );
        void applyWindow( float*, float*, int );
        void computeSecondOrderLowPassParameters( float, float, float*, float* );
        float processSecondOrderFilter( float, float*, float*, float* );
        float a[2], b[3], mem1[4], mem2[4];
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
        void (*FreqCallback)(float *, int);
};