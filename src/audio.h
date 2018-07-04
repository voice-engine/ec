
#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "conf.h"


int capture_start(conf_t *conf);
int capture_stop();
int capture_read(void *buf, size_t frames, int timeout_ms);
int capture_skip(size_t frames);

int playback_start(conf_t *conf);
int playback_stop();
int playback_read(void *buf, size_t frames, int timeout_ms);

#endif // _AUDIO_H_
