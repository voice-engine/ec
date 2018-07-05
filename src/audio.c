// audio.c

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <error.h>
#include <sys/stat.h>

#include <alsa/asoundlib.h>

#include "pa_ringbuffer.h"
#include "audio.h"
#include "conf.h"
#include "util.h"

PaUtilRingBuffer g_playback_ringbuffer;
PaUtilRingBuffer g_capture_ringbuffer;

static pthread_t g_playback_thread;
static pthread_t g_capture_thread;

extern int g_is_quit;


int set_params(snd_pcm_t *handle, unsigned rate, unsigned channels, unsigned chunk_size)
{
    int err;
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
    if ((float)rate * 1.05 < new_rate || (float)rate * 0.95 > new_rate)
    {
        fprintf(stderr, "sample rate %d not support\n", rate);
        exit(1);
    }

    err = snd_pcm_hw_params_set_channels(handle, hw_params, channels);
    assert(err >= 0);

    err = snd_pcm_hw_params_set_buffer_size(handle, hw_params, chunk_size * 2);
    assert(err >= 0);

    err = snd_pcm_hw_params_set_period_size(handle, hw_params, chunk_size, 0);
    assert(err >= 0);

    err = snd_pcm_hw_params(handle, hw_params);
    if (err < 0)
    {
        fprintf(stderr, "Unable to install hw params:");
        exit(1);
    }

    // snd_pcm_hw_params_free(&hw_params);

    return 0;
}

void *playback(void *ptr)
{
    int err;
    unsigned chunk_bytes;
    unsigned frame_bytes;
    char *chunk = NULL;
    snd_pcm_t *handle;
    unsigned chunk_size = 1024;
    unsigned zero_count = 0;
    conf_t *conf = (conf_t *)ptr;

    if ((err = snd_pcm_open(&handle, conf->out_pcm, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        fprintf(stderr, "cannot open audio device %s (%s)\n",
                conf->out_pcm,
                snd_strerror(err));
        exit(1);
    }

    set_params(handle, conf->rate, conf->ref_channels, chunk_size);

    frame_bytes = conf->ref_channels * 2;
    chunk_bytes = chunk_size * frame_bytes;
    chunk = (char *)malloc(chunk_bytes);
    if (chunk == NULL)
    {
        fprintf(stderr, "not enough memory\n");
        exit(1);
    }

    struct stat st;

    if (stat(conf->playback_fifo, &st) != 0) {
        mkfifo(conf->playback_fifo, 0666);
    } else if (!S_ISFIFO(st.st_mode)) {
        remove(conf->playback_fifo);
        mkfifo(conf->playback_fifo, 0666);
    }

    int fd = open(conf->playback_fifo, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        fprintf(stderr, "failed to open %s, error %d\n", conf->playback_fifo, fd);
        exit(1);
    }
    long pipe_size = (long)fcntl(fd, F_GETPIPE_SZ);
    if (pipe_size == -1)
    {
        perror("get pipe size failed.");
    }
    printf("default pipe size: %ld\n", pipe_size);

    int ret = fcntl(fd, F_SETPIPE_SZ, chunk_bytes * 4);
    if (ret < 0)
    {
        perror("set pipe size failed.");
    }

    pipe_size = (long)fcntl(fd, F_GETPIPE_SZ);
    if (pipe_size == -1)
    {
        perror("get pipe size 2 failed.");
    }
    printf("new pipe size: %ld\n", pipe_size);

    int wait_us = chunk_size * 1000000 / conf->rate / 4;
    while (!g_is_quit)
    {
        int count = 0;

        for (int i = 0; i < 2; i++)
        {
            int result = read(fd, chunk + count, chunk_bytes - count);
            if (result < 0)
            {
                if (errno != EAGAIN)
                {
                    fprintf(stderr, "read() returned %d, errno = %d\n", result, errno);
                    exit(1);
                }
            }
            else
            {
                count += result;
            }

            if (count >= chunk_bytes)
            {
                break;
            }

            usleep(wait_us);
        }

        if (count < chunk_bytes)
        {
            memset(chunk + count, 0, chunk_bytes - count);

            if (count)
            {
                printf("playback filled %d bytes zero\n", chunk_bytes - count);
            }
        }

        if (0 == count)
        {
            // bypass AEC when no playback
            if (zero_count > (conf->filter_length + conf->buffer_size))
            {
                if (!conf->bypass)
                {
                    conf->bypass = 1;
                    printf("No playback, bypass AEC\n");
                }
            }
            else
            {
                zero_count += chunk_size;
            }
        }
        else
        {
            if (conf->bypass)
            {
                conf->bypass = 0;
                zero_count = 0;
                printf("Enable AEC\n");
            }
        }

        count = chunk_size;
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
                PaUtil_WriteRingBuffer(&g_playback_ringbuffer, data, r);
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
    unsigned frame_bytes;
    void *chunk = NULL;
    snd_pcm_t *handle;
    unsigned chunk_size = 1024;
    conf_t *conf = (conf_t *)ptr;

    if ((err = snd_pcm_open(&handle, conf->rec_pcm, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf(stderr, "cannot open audio device %s (%s)\n",
                conf->rec_pcm,
                snd_strerror(err));
        exit(1);
    }

    set_params(handle, conf->rate, conf->rec_channels, chunk_size);

    frame_bytes = conf->rec_channels * 2;
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

        if (r > 0)
        {
            ring_buffer_size_t written =
                PaUtil_WriteRingBuffer(&g_capture_ringbuffer, chunk, r);
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


int capture_start(conf_t *conf)
{
    unsigned buffer_size = power2(conf->buffer_size);
    unsigned buffer_bytes = conf->rec_channels * conf->bits_per_sample / 8;

    void *buf = calloc(buffer_size, buffer_bytes);
    if (buf == NULL)
    {
        fprintf(stderr, "Fail to allocate memory.\n");
        exit(1);
    }

    ring_buffer_size_t ret = PaUtil_InitializeRingBuffer(&g_capture_ringbuffer, buffer_bytes, buffer_size, buf);
    if (ret == -1)
    {
        fprintf(stderr, "Initialize ring buffer but element count is not a power of 2.\n");
        exit(1);
    }
    
    pthread_create(&g_capture_thread, NULL, capture, conf);

    return 0;
}

int playback_start(conf_t *conf)
{
    unsigned buffer_size = power2(conf->buffer_size);
    unsigned buffer_bytes = conf->ref_channels * conf->bits_per_sample / 8;

    void *buf = calloc(buffer_size, buffer_bytes);
    if (buf == NULL)
    {
        fprintf(stderr, "Fail to allocate memory.\n");
        exit(1);
    }

    ring_buffer_size_t ret = PaUtil_InitializeRingBuffer(&g_playback_ringbuffer, buffer_bytes, buffer_size, buf);
    if (ret == -1)
    {
        fprintf(stderr, "Initialize ring buffer but element count is not a power of 2.\n");
        exit(1);
    }

    
    pthread_create(&g_playback_thread, NULL, playback, conf);

    return 0;
}


int capture_stop()
{
    void *ret = NULL;
    pthread_join(g_capture_thread, &ret);

    free(g_capture_ringbuffer.buffer);

    return 0;
}

int playback_stop()
{
    void *ret = NULL;
    pthread_join(g_playback_thread, &ret);

    free(g_playback_ringbuffer.buffer);

    return 0;
}

int capture_read(void *buf, size_t frames, int timeout_ms)
{
    while (PaUtil_GetRingBufferReadAvailable(&g_capture_ringbuffer) < frames && timeout_ms > 0)
    {
        usleep(10);
        timeout_ms--;
    }

    return PaUtil_ReadRingBuffer(&g_capture_ringbuffer, buf, frames);
}


int capture_skip(size_t frames)
{
    while (PaUtil_GetRingBufferReadAvailable(&g_capture_ringbuffer) < frames)
    {
        usleep(1000);
    }
    return PaUtil_AdvanceRingBufferReadIndex(&g_capture_ringbuffer, frames);
}

int playback_read(void *buf, size_t frames, int timeout_ms)
{
    while (PaUtil_GetRingBufferReadAvailable(&g_playback_ringbuffer) < frames && timeout_ms > 0)
    {
        usleep(1000);
        timeout_ms--;
    }

    return PaUtil_ReadRingBuffer(&g_playback_ringbuffer, buf, frames);
}
