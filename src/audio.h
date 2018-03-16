
#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <pa_ringbuffer.h>

#define CAPTURE_INDEX       0
#define PLAYBACK_INDEX      1
#define PLAYED_INDEX        2
#define PROCESSED_INDEX     3

extern char *g_playback_device;
extern char *g_capture_device;

extern PaUtilRingBuffer g_ringbuffer[];

void audio_start(int sample_rate, int channels, int ring_buffer_size);
void audio_stop();


#endif // _AUDIO_H_
