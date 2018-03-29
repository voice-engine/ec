
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include <pa_ringbuffer.h>


extern int g_is_quit;

char *playback_fifo = "/tmp/ec.input";
char *capture_fifo = "/tmp/ec.output";

void *fifo_read(void *ptr)
{
    ring_buffer_size_t size1, size2, available;
    void *data1, *data2;
    PaUtilRingBuffer *ringbuffer = ptr;
    int fd = open(playback_fifo, O_RDONLY);
    if (fd < 0) {
        printf("failed to open %s, error %d\n", playback_fifo, fd);
        return NULL;
    }

    while (!g_is_quit)
    {
        available = PaUtil_GetRingBufferWriteAvailable(ringbuffer);
        PaUtil_GetRingBufferWriteRegions(ringbuffer, available, &data1, &size1, &data2, &size2);
        if (size1 > 0) {
            int result = read(fd, data1, size1 * ringbuffer->elementSizeBytes);
            // printf("read %d of %d\n", result / 2, size1);
            if (result > 0) {
                PaUtil_AdvanceRingBufferWriteIndex(ringbuffer, result / ringbuffer->elementSizeBytes);
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

void *fifo_write(void *ptr)
{
    ring_buffer_size_t size1, size2, available;
    void *data1, *data2;
    PaUtilRingBuffer *ringbuffer = ptr;
    int fd = open(capture_fifo, O_WRONLY);      // will block until reader is available
    if (fd < 0) {
        printf("failed to open %s, error %d\n", capture_fifo, fd);
        return NULL;
    }

    // clear
    PaUtil_AdvanceRingBufferReadIndex(ringbuffer, PaUtil_GetRingBufferReadAvailable(ringbuffer));
    while (!g_is_quit)
    {
        available = PaUtil_GetRingBufferReadAvailable(ringbuffer);
        PaUtil_GetRingBufferReadRegions(ringbuffer, available, &data1, &size1, &data2, &size2);
        if (size1 > 0) {
            int result = write(fd, data1, size1 * ringbuffer->elementSizeBytes);
            // printf("write %d of %d\n", result / 2, size1);
            if (result > 0) {
                PaUtil_AdvanceRingBufferReadIndex(ringbuffer, result / ringbuffer->elementSizeBytes);
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

int fifo_setup(PaUtilRingBuffer *playback, PaUtilRingBuffer *capture)
{
    // pthread_t reader;
    pthread_t writer;
    struct stat st;

    if (stat(playback_fifo, &st) != 0) {
        mkfifo(playback_fifo, 0666);
    } else if (!S_ISFIFO(st.st_mode)) {
        remove(playback_fifo);
        mkfifo(playback_fifo, 0666);
    }

    if (stat(capture_fifo, &st) != 0) {
        mkfifo(capture_fifo, 0666);
    } else if (!S_ISFIFO(st.st_mode)) {
        remove(capture_fifo);
        mkfifo(capture_fifo, 0666);
    }

    // pthread_create(&reader, NULL, fifo_read, playback);
    pthread_create(&writer, NULL, fifo_write, capture);

    return 0;
}

int fifo_clear(void)
{
    unlink(playback_fifo);
    unlink(capture_fifo);

    return 0;
}