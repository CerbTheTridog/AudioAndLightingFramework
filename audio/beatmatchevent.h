//#include "beatmatch.h"
#include "lib/libfft.h"

class BeatMatchEvent : public BeatMatch
{
	public:
		BeatMatchEvent(int, int, int);
		void EventThread();
};