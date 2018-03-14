
#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <pa_util.h>
#include <pa_ringbuffer.h>

#define CAPTURE_INDEX       0
#define PLAYBACK_INDEX      1
#define PLAYED_INDEX        2
#define PROCESSED_INDEX     3

extern PaUtilRingBuffer g_ringbuffer[];


void audio_list();
void audio_start(int in_device, int out_device, int sample_rate, int num_channels, int frame_count);
void audio_stop();


#endif // _AUDIO_H_
