#ifndef __BEATMATCHEVENT_H__
#define __BEATMATCHEVEN_H__
#include "lib/libfft.h"

class BeatMatchEvent : public BeatMatch
{
		void (*FreqCallback)(float *, int);
		void (*Callback)(int, int);
		int BeatHz;
		float *BeatWindow;
		int *Beats;
		int *ActualBeats;
		bool IgnoreRepeatBeat;
		int BeatWindowSize;
		float ***NoteIndexes;
		float Decibel(float);
	public:
		BeatMatchEvent(int, int, int, int, int, bool, void (*)(float*, int), void (*)(int, int));
		void EventThread();
};
#endif
