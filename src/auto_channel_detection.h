#ifndef _AUTO_CHANNEL_DETECTION_H_
#define _AUTO_CHANNEL_DETECTION_H_

void auto_channel_detection(
    conf_t *config,               // global config object
    int16_t *rec,                 // pre-allocated rec buffer
    int *mic_list,                // buffer which will contain the list of the discovered mic channels
    int *loopback_list,           // buffer which will contain the list of the discovered loopbacks channels
    int frame_size,               // capturing buffer's frame size
    int timeout,                  // capturing buffer's timeout
    int coherence_window_size_ms, // how long should discovered the channels be consistent
    int total_window_size_ms,     // how long before we give up after discovering inconsistent results
    int envelope_ms,              // influences the decay rate of the envelope's window used to detect channel volume
    int save_audio                // save audio captured during channel detection operations
);

#endif // _AUTO_CHANNEL_DETECTION_H_
