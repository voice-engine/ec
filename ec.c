
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/epoll.h>

#include <speex/speex_echo.h>
#include <mosquitto.h>

#include "ring.h"
#include "decimate.h"

#define MAX_EVENTS 1
#define DELAY_N (3970 + 425)
#define BLOCK_SIZE 160
#define M          3        // 48000 Hz to 16000 Hz, decimation factor is 3
#define NUMTAPS 545

#define CAPTURE_PIPE "/var/tmp/ec/capture.pipe"
#define PLAYBACK_PIPE "/var/tmp/ec/playback.pipe"

#define DELTA(t1, t2) ((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_nsec - t1.tv_nsec) / 1000)


volatile int g_quit = 0;
volatile int g_bypass = 0;
volatile int g_debug = 0;

FILE *fp_far = NULL;
FILE *fp_near = NULL;
FILE *fp_out = NULL;

void handle_sigint(int sig)
{
    printf("\nquit...\n");
    g_quit = 1;
}

void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
    if (message->payloadlen)
    {
        printf("%s %s\n", message->topic, (char *)(message->payload));
        if (strcmp(message->topic, "/voicen/ec/bypass") == 0)
        {
            g_bypass = '1' == *(char *)(message->payload) ? 1 : 0;
        }
        else if (strcmp(message->topic, "/voicen/ec/debug") == 0)
        {
            g_debug = '1' == *(char *)(message->payload) ? 1 : 0;
            if (g_debug)
            {
                if (NULL == fp_far)
                {
                    fp_far = fopen("/var/tmp/ec/far.raw", "w");
                    fp_near = fopen("/var/tmp/ec/near.raw", "w");
                    fp_out = fopen("/var/tmp/ec/out.raw", "w");
                }
            }
            else if (fp_far)
            {
                fclose(fp_far);
                fclose(fp_near);
                fclose(fp_out);
            }
        }
    }
    else
    {
        printf("%s (null)\n", message->topic);
    }
    // fflush(stdout);
}

void connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    int i;
    if (!result)
    {
        /* Subscribe to broker information topics on successful connect. */
        mosquitto_subscribe(mosq, NULL, "/voicen/ec/#", 2);
    }
    else
    {
        fprintf(stderr, "Connect failed\n");
    }
}

void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
    int i;

    printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
    for (i = 1; i < qos_count; i++)
    {
        printf(", %d", granted_qos[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    {
        int pid = getpid();
        FILE *fp = fopen("/var/tmp/ec/pid", "w");
        fprintf(fp, "%d\n", pid);
        fclose(fp);
    }

    int result;

    int last_read = 0;
    int last_write = 0;

    char *aplay = "aplay -M -D hw:Codec -t raw -c 1 -r 48000 -f S32_LE --period-size 960 -v -";
    char *arecord = "arecord -M -D ac108 -t raw -c 1 -r 16000 -f S16_LE --period-size 160 --buffer-size=1240 -v -";
    int channels_far = 1;
    int channels_near = 1;
    int rate = 16000;
    int block_bytes_16k1ch16 = BLOCK_SIZE * 1 * sizeof(int16_t);
    int block_bytes_48k1ch32 = BLOCK_SIZE * 3 * sizeof(int32_t);
    int filter_length = 4096;

    int effect_blocks = (filter_length + DELAY_N) / BLOCK_SIZE + 8;
    int effect = 0;

    int16_t buffer_near[BLOCK_SIZE * channels_near];
    int16_t buffer_out[BLOCK_SIZE * channels_near];
    int16_t *buf;

    decimate_t S;
    int32_t src[BLOCK_SIZE * M];              // A source array of input data
    int32_t dst[BLOCK_SIZE];                  // A destination array for the transformed data
    int32_t decimate_buffer[NUMTAPS + BLOCK_SIZE * M - 1]; // A "state" buffer for use within the FIR

    decimate_init(&S, NUMTAPS, M, coeffs, decimate_buffer, BLOCK_SIZE * M);

    FILE *fplay = NULL;
    FILE *frecord = NULL;
    int fin = -1;
    int fout = -1;

    const char *capture_pipe = CAPTURE_PIPE;
    const char *playback_pipe = PLAYBACK_PIPE;

    ring_t *ring = ring_new(20);
    ring_head_advance(ring, DELAY_N * 2);

    char *host = "localhost";
    int port = 1883;
    int keepalive = 60;
    bool clean_session = true;
    struct mosquitto *mosq = NULL;

    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, clean_session, NULL);
    if (!mosq)
    {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }
    mosquitto_connect_callback_set(mosq, connect_callback);
    mosquitto_message_callback_set(mosq, message_callback);
    mosquitto_subscribe_callback_set(mosq, subscribe_callback);

    if (mosquitto_connect(mosq, host, port, keepalive))
    {
        fprintf(stderr, "Unable to connect.\n");
        return 1;
    }

    signal(SIGINT, handle_sigint);
    do
    {
        SpeexEchoState *echo_state;
        echo_state = speex_echo_state_init_mc(BLOCK_SIZE,
                                              filter_length,
                                              channels_near,
                                              channels_far);
        speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &rate);

        if ((fin = open(playback_pipe, O_RDWR | O_NONBLOCK)) < 0)
        {
            fprintf(stderr, "Failed to open: %s\n", CAPTURE_PIPE);
            break;
        }

        if ((fout = open(capture_pipe, O_RDWR | O_NONBLOCK)) < 0)
        {
            fprintf(stderr, "Failed to open: %s\n", PLAYBACK_PIPE);
            break;
        }

        if ((frecord = popen(arecord, "r")) == NULL)
        {
            fprintf(stderr, "Failed to run: %s\n", arecord);
            break;
        }

        if ((fplay = popen(aplay, "w")) == NULL)
        {
            fprintf(stderr, "Failed to run: %s\n", aplay);
            break;
        }

        fread(buffer_near, sizeof(int16_t) * channels_near, BLOCK_SIZE, frecord);

        memset(src, 0, block_bytes_48k1ch32);
        for (int i=0; i<16; i++)
        {
            fwrite(src, sizeof(int32_t), BLOCK_SIZE * M, fplay);

            buf = ring_head(ring);
            memset(buf, 0, block_bytes_16k1ch16);
            ring_head_advance(ring, block_bytes_16k1ch16);
        }


        struct epoll_event event, events[MAX_EVENTS];
        int epoll_fd = epoll_create1(0);

        event.events = EPOLLIN;
        event.data.fd = fin;

        if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fin, &event))
        {
            printf("Failed to add file descriptor to epoll\n");
            break;
        }

        struct timespec t0, t1, t2, t3, t4, t5;
        while (!g_quit)
        {
            clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

            int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 0);
            if (event_count > 0)
            {
                result = read(fin, src, block_bytes_48k1ch32);

                // zero check
                int nonzero = 0;
                for (int i=0; i<result; i+=4)
                {
                    if (0 != src[i])
                    {
                        nonzero = 1;
                        if (result < block_bytes_48k1ch32)
                        {
                            memset((uint8_t *)src + result, 0, block_bytes_48k1ch32 - result);
                        }
                        else
                        {
                            if (0 >= effect)
                            {
                                char *message = "1";
                                mosquitto_publish(mosq, NULL, "/voicen/amp", 1, message, 0, 0);
                            }
                            effect = effect_blocks;
                        }
                        break;
                    }
                }

                if (0 > result)
                {
                    memset(src, 0, block_bytes_48k1ch32);
                }
                else if (0 == nonzero)
                {
                    result = 0;
                }
            }
            else
            {
                result = 0;
                memset(src, 0, block_bytes_48k1ch32);
            }

            if (result != last_read)
            {
                last_read = result;
                printf("r %d\n", result);
            }

            clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
            fwrite(src, sizeof(int32_t), BLOCK_SIZE * M, fplay);

            int processing = g_debug || ((last_write > 0) && (effect > 0) && !g_bypass);

            if (0 < effect)
            {
                effect--;
                if (0 >= effect)
                {
                    char *message = "0";
                    mosquitto_publish(mosq, NULL, "/voicen/amp", 1, message, 0, 0);
                }
            }

            buf = ring_head(ring);
            if (processing)
            {
                decimate_process(&S, src, dst);

                for (int i = 0; i < BLOCK_SIZE; i++)
                {
                    buf[i] = ((int16_t *)(dst + i))[1];
                }
            }
            else
            {
                memset(buf, 0, block_bytes_16k1ch16);
            }
            ring_head_advance(ring, block_bytes_16k1ch16);

            clock_gettime(CLOCK_MONOTONIC_RAW, &t2);

            fread(buffer_near, sizeof(int16_t) * channels_near, BLOCK_SIZE, frecord);

            clock_gettime(CLOCK_MONOTONIC_RAW, &t3);

            buf = ring_tail(ring);
            ring_tail_advance(ring, block_bytes_16k1ch16);

            if (processing)
            {
                speex_echo_cancellation(echo_state, buffer_near, buf, buffer_out);

                clock_gettime(CLOCK_MONOTONIC_RAW, &t4);

                result = write(fout, buffer_out, channels_near * sizeof(int16_t) * BLOCK_SIZE);

                if (g_debug)
                {
                    fwrite(buf, sizeof(int16_t) * channels_far, BLOCK_SIZE, fp_far);
                    fwrite(buffer_near, sizeof(int16_t) * channels_near, BLOCK_SIZE, fp_near);
                    fwrite(buffer_out, sizeof(int16_t) * channels_near, BLOCK_SIZE, fp_out);
                }
            }
            else
            {
                clock_gettime(CLOCK_MONOTONIC_RAW, &t4);
                result = write(fout, buffer_near, channels_near * sizeof(int16_t) * BLOCK_SIZE);
            }

            if (result != last_write)
            {
                last_write = result;
                printf("w %d\n", result);
            }


            // mosquitto_publish(mosq, NULL, "/ec/message", msglen,  message, 0, 0);
            mosquitto_loop(mosq, 0, 1);

            clock_gettime(CLOCK_MONOTONIC_RAW, &t5);

            if (DELTA(t0, t5) > 12000)
            printf("dt:\t%ld,\t%ld,\t%ld,\t%ld,\t%ld\n", DELTA(t0, t1), DELTA(t1, t2), DELTA(t2, t3), DELTA(t3, t4), DELTA(t0, t5));
        }

        close(epoll_fd);
    } while (0);

    {
        char *message = "0";
        mosquitto_publish(mosq, NULL, "/voicen/amp", 1, message, 0, 0);
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    ring_free(ring);

    remove("/var/tmp/ec/pid");

    if (fin >= 0)
        close(fin);
    if (fout >= 0)
        close(fout);
    if (fplay)
        pclose(fplay);
    if (frecord)
        pclose(frecord);

    if (fp_far)
    {
        fclose(fp_far);
        fclose(fp_near);
        fclose(fp_out);
    }

    return 0;
}
