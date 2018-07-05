
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "pa_ringbuffer.h"
#include "conf.h"
#include "util.h"

extern int g_is_quit;

PaUtilRingBuffer g_out_ringbuffer;

void *fifo_thread(void *ptr)
{
    conf_t *conf = (conf_t *)ptr;
    ring_buffer_size_t size1, size2, available;
    void *data1, *data2;
    int fd = open(conf->out_fifo, O_WRONLY);      // will block until reader is available
    if (fd < 0) {
        printf("failed to open %s, error %d\n", conf->out_fifo, fd);
        return NULL;
    }

    // clear
    PaUtil_AdvanceRingBufferReadIndex(&g_out_ringbuffer, PaUtil_GetRingBufferReadAvailable(&g_out_ringbuffer));
    while (!g_is_quit)
    {
        available = PaUtil_GetRingBufferReadAvailable(&g_out_ringbuffer);
        PaUtil_GetRingBufferReadRegions(&g_out_ringbuffer, available, &data1, &size1, &data2, &size2);
        if (size1 > 0) {
            int result = write(fd, data1, size1 * g_out_ringbuffer.elementSizeBytes);
            // printf("write %d of %d\n", result / 2, size1);
            if (result > 0) {
                PaUtil_AdvanceRingBufferReadIndex(&g_out_ringbuffer, result / g_out_ringbuffer.elementSizeBytes);
            } else {
                sleep(1);
            }
        } else {
            usleep(100000);
        }
    }

    close(fd);

    return NULL;
}

int fifo_setup(conf_t *conf)
{
    pthread_t writer;
    struct stat st;

    unsigned buffer_size = power2(conf->buffer_size);
    unsigned buffer_bytes = conf->out_channels * conf->bits_per_sample / 8;

    void *buf = calloc(buffer_size, buffer_bytes);
    if (buf == NULL)
    {
        fprintf(stderr, "Fail to allocate memory.\n");
        exit(1);
    }

    ring_buffer_size_t ret = PaUtil_InitializeRingBuffer(&g_out_ringbuffer, buffer_bytes, buffer_size, buf);
    if (ret == -1)
    {
        fprintf(stderr, "Initialize ring buffer but element count is not a power of 2.\n");
        exit(1);
    }

    if (stat(conf->out_fifo, &st) != 0) {
        mkfifo(conf->out_fifo, 0666);
    } else if (!S_ISFIFO(st.st_mode)) {
        remove(conf->out_fifo);
        mkfifo(conf->out_fifo, 0666);
    }

    pthread_create(&writer, NULL, fifo_thread, conf);

    return 0;
}


int fifo_write(void *buf, size_t frames)
{
    return PaUtil_WriteRingBuffer(&g_out_ringbuffer, buf, frames);
}
