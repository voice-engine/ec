// audio.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <portaudio.h>

#include <audio.h>


const int bits_per_sample = 16;     // only support 16 bits sample width
const int g_output_channels = 1;    // only support mono output
int g_input_channels = 2;


PaUtilRingBuffer g_ringbuffer[4];

// Pointer to PortAudio stream.
PaStream *g_pa_stream;
PaStream *g_playback_stream;

pthread_t g_playback_thread;

extern int g_is_quit;

int audio_callback(const void *input,
                      void *output,
                      unsigned long frame_count,
                      const PaStreamCallbackTimeInfo *time_info,
                      PaStreamCallbackFlags status_flags,
                      void *user_data)
{
    ring_buffer_size_t written =
            PaUtil_WriteRingBuffer(&g_ringbuffer[CAPTURE_INDEX], input, frame_count);
    if (written < frame_count) {
        printf("lost %ld frames\n", frame_count - written);
    }

    return paContinue;
}

int playback_callback(const void *input,
                      void *output,
                      unsigned long frame_count,
                      const PaStreamCallbackTimeInfo *time_info,
                      PaStreamCallbackFlags status_flags,
                      void *user_data)
{
    ring_buffer_size_t readn = PaUtil_ReadRingBuffer(&g_ringbuffer[PLAYBACK_INDEX], output, frame_count);
    if (readn < frame_count) {
        int frame_bytes = g_output_channels * bits_per_sample / 8;
        memset((int8_t *)output + readn * frame_bytes, 0, (frame_count - readn) * frame_bytes);
        if (readn) {
            printf("The playback ring buffer is empty\n");
        }
    }
    PaUtil_WriteRingBuffer(&g_ringbuffer[PLAYED_INDEX], output, frame_count);

    return paContinue;
}

void *play(void *ptr)
{
    PaError err;
    ring_buffer_size_t frame_count = 1600;
    void *output = NULL;
    int frame_bytes = g_output_channels * bits_per_sample / 8;

    output = calloc(frame_count * g_output_channels, bits_per_sample / 8);
    if (output == NULL) {
        fprintf(stderr, "Fail to allocate memory.\n");
        exit(1);
    }

    err = Pa_StartStream(g_playback_stream);
    if (err != paNoError)
    {
        fprintf(stderr, "Fail to start PortAudio stream, error message is %s.\n",
                Pa_GetErrorText(err));
        exit(1);
    }

    while (!g_is_quit) {
        ring_buffer_size_t readn = PaUtil_ReadRingBuffer(&g_ringbuffer[PLAYBACK_INDEX], output, frame_count);
        if (readn < frame_count) {
            memset((int8_t *)output + readn * frame_bytes, 0, (frame_count - readn) * frame_bytes);
            if (readn) {
                printf("The playback ring buffer is empty\n");
            }
        }

        err = Pa_WriteStream(g_playback_stream, output, frame_count);
        if (err) {
            fprintf(stderr, "XRUN\n");
        }

        PaUtil_WriteRingBuffer(&g_ringbuffer[PLAYED_INDEX], output, frame_count);
    }


    Pa_StopStream(g_playback_stream);

    return NULL;
}

void audio_start(int capture_device, int playback_device, int sample_rate, int num_channels, int frame_count)
{
    PaStreamParameters out_param;
    PaStreamParameters in_param;
    int frame_bytes[4];
    int frames_per_buffer = sample_rate * 100 / 1000;   // 100 ms

    g_input_channels = num_channels;

    frame_bytes[PLAYBACK_INDEX] = g_output_channels * bits_per_sample / 8;
    frame_bytes[PLAYED_INDEX] = g_output_channels * bits_per_sample / 8;
    frame_bytes[CAPTURE_INDEX] = g_input_channels * bits_per_sample / 8;
    frame_bytes[PROCESSED_INDEX] = g_input_channels * bits_per_sample / 8;


    for (int i=0; i<sizeof(g_ringbuffer)/sizeof(g_ringbuffer[0]); i++) {
        void *buf = calloc(frame_count, frame_bytes[i]);
        if (buf == NULL) {
            fprintf(stderr, "Fail to allocate memory.\n");
            exit(1);
        }

        ring_buffer_size_t ret = PaUtil_InitializeRingBuffer(&g_ringbuffer[i], frame_bytes[i], frame_count, buf);
        if (ret == -1) {
            fprintf(stderr, "Initialize ring buffer but element count is not a power of 2.\n");
            exit(1);
        }
    }

    // Initializes PortAudio.
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "Fail to initialize PortAudio, error message is %s.\n",
                Pa_GetErrorText(err));
        exit(1);
    }

    in_param.channelCount = num_channels;
    in_param.device = capture_device;
    in_param.hostApiSpecificStreamInfo = NULL;
    in_param.sampleFormat = paInt16;
    in_param.suggestedLatency = Pa_GetDeviceInfo(capture_device)->defaultLowInputLatency;


    out_param.channelCount = g_output_channels;
    out_param.device = playback_device;
    out_param.hostApiSpecificStreamInfo = NULL;
    out_param.sampleFormat = paInt16;
    out_param.suggestedLatency = Pa_GetDeviceInfo(playback_device)->defaultLowOutputLatency;

    // PaError pa_open_ans = Pa_OpenDefaultStream(
    //     &g_pa_stream, num_channels, 1, paInt16, sample_rate,
    //     frames_per_buffer, audio_callback, NULL);

    err = Pa_OpenStream(
        &g_pa_stream,
        &in_param,
        NULL,
        sample_rate,
        frames_per_buffer,
        paNoFlag,
        audio_callback,
        NULL);

    if (err != paNoError)
    {
        fprintf(stderr, "Fail to open PortAudio stream, error message is %s.\n",
                Pa_GetErrorText(err));
        exit(1);
    }

    err = Pa_OpenStream(
        &g_playback_stream,
        NULL,
        &out_param,
        sample_rate,
        frames_per_buffer,
        paClipOff,
        NULL,
        NULL);

    if (err != paNoError)
    {
        fprintf(stderr, "Fail to open PortAudio stream, error message is %s.\n",
                Pa_GetErrorText(err));
        exit(1);
    }

    pthread_create(&g_playback_thread, NULL, play, NULL);

    err = Pa_StartStream(g_pa_stream);
    if (err != paNoError)
    {
        fprintf(stderr, "Fail to start PortAudio stream, error message is %s.\n",
                Pa_GetErrorText(err));
        exit(1);
    }
}

void audio_stop()
{
    void* ret = NULL;

    Pa_StopStream(g_pa_stream);
    Pa_CloseStream(g_pa_stream);

    pthread_join(g_playback_thread, &ret);
    Pa_CloseStream(g_playback_stream);

    Pa_Terminate();

    for (int i=0; i<sizeof(g_ringbuffer)/sizeof(g_ringbuffer[0]); i++) {
        free(g_ringbuffer[i].buffer);
    }
}

void audio_list()
{
    // Initializes PortAudio.
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "Fail to initialize PortAudio, error message is %s.\n",
                Pa_GetErrorText(err));
        exit(1);
    }

    int num_devices = Pa_GetDeviceCount();
    printf("-------------------------------------------------------\n");
    printf("Index | Max Input Channels | Max Output Channels | Name\n");
    printf("-------------------------------------------------------\n");
    for (int i=0; i<num_devices; i++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        printf("%5d | %3d | %3d | %s\n", i, info->maxInputChannels, info->maxOutputChannels, info->name);
    }

    Pa_Terminate();
}
