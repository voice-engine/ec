// audio.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include <audio.h>

const unsigned bits_per_sample = 16;  // only support 16 bits sample width
const unsigned g_output_channels = 1; // only support mono output
unsigned g_input_channels = 2;
unsigned g_sample_rate = 16000;

PaUtilRingBuffer g_ringbuffer[4];

char *g_playback_device = "default";
char *g_capture_device = "default";

static pthread_t g_playback_thread;
static pthread_t g_capture_thread;

static snd_output_t *log;

extern int g_is_quit;

unsigned set_params(snd_pcm_t *handle, unsigned rate, unsigned channels)
{
    int err;
    unsigned buffer_time = 0;
    unsigned period_time = 0;
    unsigned long chunk_size = 0;
    snd_pcm_hw_params_t *hw_params;
    unsigned new_rate = rate;

    err = snd_pcm_hw_params_malloc(&hw_params);
    assert(err >= 0);

    err = snd_pcm_hw_params_any(handle, hw_params);
    assert(err >= 0);

    err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    assert(err >= 0);

    err = snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
    assert(err >= 0);

    err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &new_rate, 0);
    assert(err >= 0);
    if ((float)rate * 1.05 < new_rate || (float)rate * 0.95 > new_rate) {
        fprintf(stderr, "sample rate %d not support\n", rate);
        exit(1);
    }

    err = snd_pcm_hw_params_set_channels(handle, hw_params, channels);
    assert(err >= 0);

    err = snd_pcm_hw_params_get_buffer_time_max(hw_params, &buffer_time, 0);
    assert(err >= 0);
    if (buffer_time > 500000)
        buffer_time = 500000;

    if (buffer_time > 0)
    {
        err = snd_pcm_hw_params_set_buffer_time_near(handle, hw_params, &buffer_time, 0);
        assert(err >= 0);

        period_time = buffer_time / 4;
        err = snd_pcm_hw_params_set_period_time_near(handle, hw_params, &period_time, 0);
        assert(err >= 0);
    }

    err = snd_pcm_hw_params(handle, hw_params);
    if (err < 0)
    {
        fprintf(stderr, "Unable to install hw params:");
        snd_pcm_hw_params_dump(hw_params, log);
        exit(1);
    }

    snd_pcm_hw_params_get_period_size(hw_params, &chunk_size, 0);
    if (chunk_size == 0)
    {
        chunk_size = 256;
    }

    // snd_pcm_hw_params_free(&hw_params);

    return chunk_size;
}

void *playback(void *ptr)
{
    int err;
    unsigned chunk_size = 0;
    unsigned frame_bytes;
    void *chunk = NULL;
    snd_pcm_t *handle;

    if ((err = snd_pcm_open(&handle, g_playback_device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        fprintf(stderr, "cannot open audio device %s (%s)\n",
                g_playback_device,
                snd_strerror(err));
        exit(1);
    }

    chunk_size = set_params(handle, g_sample_rate, g_output_channels);

    frame_bytes = g_output_channels * 2;
    chunk = malloc(chunk_size * frame_bytes);
    if (chunk == NULL)
    {
        fprintf(stderr, "not enough memory\n");
        exit(1);
    }

    while (!g_is_quit)
    {
        ring_buffer_size_t readn = PaUtil_ReadRingBuffer(&g_ringbuffer[PLAYBACK_INDEX], chunk, chunk_size);
        if (readn < chunk_size)
        {
            memset((char *)chunk + readn * frame_bytes, 0, (chunk_size - readn) * frame_bytes);
            if (readn)
            {
                printf("playback ring buffer is empty\n");
            }
        }

        size_t count = chunk_size;
        char *data = (char *)chunk;
        while (count > 0 && !g_is_quit)
        {
            ssize_t r = snd_pcm_writei(handle, data, count);
            if (r == -EAGAIN || (r >= 0 && (size_t)r < count))
            {
                snd_pcm_wait(handle, 100);
            }
            else if (r == -EPIPE)
            {
                fprintf(stderr, "underrun\n");
                exit(1);
            }
            else if (r < 0)
            {
                fprintf(stderr, "write error: %s\n", snd_strerror(r));
                exit(1);
            }
            if (r > 0)
            {
                PaUtil_WriteRingBuffer(&g_ringbuffer[PLAYED_INDEX], data, r);
                count -= r;
                data += r * frame_bytes;
            }
        }
    }

    snd_pcm_close(handle);
    free(chunk);

    return NULL;
}

void *capture(void *ptr)
{
    int err;
    unsigned chunk_size = 0;
    unsigned frame_bytes;
    void *chunk = NULL;
    snd_pcm_t *handle;

    if ((err = snd_pcm_open(&handle, g_capture_device, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf(stderr, "cannot open audio device %s (%s)\n",
                g_capture_device,
                snd_strerror(err));
        exit(1);
    }

    chunk_size = set_params(handle, g_sample_rate, g_input_channels);

    frame_bytes = g_input_channels * 2;
    chunk = malloc(chunk_size * frame_bytes);
    if (chunk == NULL)
    {
        fprintf(stderr, "not enough memory\n");
        exit(1);
    }

    while (!g_is_quit)
    {
        ssize_t r = snd_pcm_readi(handle, chunk, chunk_size);
        if (r == -EAGAIN || (r >= 0 && (size_t)r < chunk_size))
        {
            snd_pcm_wait(handle, 100);
        }
        else if (r == -EPIPE)
        {
            fprintf(stderr, "overrun\n");
            exit(1);
        }
        else if (r < 0)
        {
            fprintf(stderr, "read error: %s\n", snd_strerror(r));
            exit(1);
        }

        if (r > 0) {
            ring_buffer_size_t written =
                PaUtil_WriteRingBuffer(&g_ringbuffer[CAPTURE_INDEX], chunk, r);
            if (written < r)
            {
                printf("lost %ld frames\n", r - written);
            }
        }

    }

    snd_pcm_close(handle);
    free(chunk);

    return NULL;
}

void audio_start(int sample_rate, int channels, int ring_buffer_size)
{
    int err;
    int buf_bytes[4];

    g_sample_rate = sample_rate;
    g_input_channels = channels;

    buf_bytes[PLAYBACK_INDEX] = g_output_channels * bits_per_sample / 8;
    buf_bytes[PLAYED_INDEX] = g_output_channels * bits_per_sample / 8;
    buf_bytes[CAPTURE_INDEX] = g_input_channels * bits_per_sample / 8;
    buf_bytes[PROCESSED_INDEX] = g_input_channels * bits_per_sample / 8;

    for (int i = 0; i < sizeof(g_ringbuffer) / sizeof(g_ringbuffer[0]); i++)
    {
        void *buf = calloc(ring_buffer_size, buf_bytes[i]);
        if (buf == NULL)
        {
            fprintf(stderr, "Fail to allocate memory.\n");
            exit(1);
        }

        ring_buffer_size_t ret = PaUtil_InitializeRingBuffer(&g_ringbuffer[i], buf_bytes[i], ring_buffer_size, buf);
        if (ret == -1)
        {
            fprintf(stderr, "Initialize ring buffer but element count is not a power of 2.\n");
            exit(1);
        }
    }

    err = snd_output_stdio_attach(&log, stderr, 0);
    assert(err >= 0);

    pthread_create(&g_playback_thread, NULL, playback, NULL);
    pthread_create(&g_capture_thread, NULL, capture, NULL);
}

void audio_stop()
{
    void *ret = NULL;

    pthread_join(g_playback_thread, &ret);
    pthread_join(g_capture_thread, &ret);

    for (int i = 0; i < sizeof(g_ringbuffer) / sizeof(g_ringbuffer[0]); i++)
    {
        free(g_ringbuffer[i].buffer);
    }
}
