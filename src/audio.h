
#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "conf.h"
#include "pa_ringbuffer.h"

#define CAPTURE_INDEX       0
#define PLAYED_INDEX        1
#define PROCESSED_INDEX     2
#define PLAYBACK_INDEX      3


extern PaUtilRingBuffer g_ringbuffer[];

void audio_start(conf_t *conf);
void audio_stop();


#endif // _AUDIO_H_
