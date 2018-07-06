// ec - echo canceller

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#include <speex/speex_echo.h>

#include "conf.h"
#include "audio.h"

const char *usage =
    "Usage:\n %s -c {input channels} -l {loopback channel} -m {mic channel list} [options]\n"
    "Options:\n"
    " -i PCM            playback PCM (default)\n"
    // " -o PCM            capture PCM (default)\n"
    " -r rate           sample rate (16000)\n"
    " -c channels       input channels\n"
    " -b size           buffer size (262144)\n"
    // " -d delay          system delay between playback and capture (0)\n"
    " -f filter_length  AEC filter length (2048)\n"
    " -l loopback       loopback channel\n"
    " -m mic_channels   microphone channel list\n"
    " -s                save audio to /tmp/recording.raw and /tmp/out.raw\n"
    " -D                daemonize\n"
    " -h                display this help text\n"
    "Note:\n"
    " Echo Cancellation with loopback channel\n"
    "  `cat /tmp/ec.output > out.raw` to get recording audio\n"
    " Only support mono playback\n";

volatile int g_is_quit = 0;

extern int fifo_setup(conf_t *conf);
extern int fifo_write(void *buf, size_t frames);

void int_handler(int signal)
{
    printf("Caught signal %d, quit...\n", signal);

    g_is_quit = 1;
}

void daemonize(void)
{
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0)
    {
        printf("fork() failed\n");
        exit(1);
    }
    /* If we got a good PID, then
        we can exit the parent process. */
    if (pid > 0)
    {
        exit(0);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0)
    {
        printf("setsid() failed\n");
        exit(1);
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0)
    {
        printf("chdir() failed\n");
        exit(1);
    }
}


int main(int argc, char *argv[])
{
    SpeexEchoState *echo_state;
    int16_t *rec = NULL;
    int16_t *near = NULL;
    int16_t *far = NULL;
    int16_t *out = NULL;
    FILE *fp_rec = NULL;
    FILE *fp_out = NULL;

    int opt = 0;
    // int delay = 0;
    int save_audio = 0;
    int daemon = 0;
    char *mic_list_str = NULL;
    int mic_list[32];
    int loopback_channel = -1;

    conf_t config = {
        .rec_pcm = "default",
        .out_pcm = "default",
        .playback_fifo = "/tmp/ec.input",
        .out_fifo = "/tmp/ec.output",
        .rate = 16000,
        .rec_channels = 0,
        .ref_channels = 1,
        .out_channels = 0,
        .bits_per_sample = 16,
        .buffer_size = 1024 * 16,
        .playback_fifo_size = 1024 * 4,
        .filter_length = 4096,
        .bypass = 0
    };

    while ((opt = getopt(argc, argv, "b:c:d:Df:hi:l:m:o:r:s")) != -1)
    {
        switch (opt)
        {
        case 'b':
            config.buffer_size = atoi(optarg);
            break;
        case 'c':
            config.rec_channels = atoi(optarg);
            break;
        // case 'd':
        //     delay = atoi(optarg);
        //     break;
        case 'D':
            daemon = 1;
            break;
        case 'f':
            config.filter_length = atoi(optarg);
            break;
        case 'h':
            printf(usage, argv[0]);
            exit(0);
        case 'i':
            config.rec_pcm = optarg;
            break;
        case 'l':
            loopback_channel = atoi(optarg);
            // loopback channel
            break;
        case 'm':
            // microphone channel list
            mic_list_str = optarg;
            break;
        // case 'o':
        //     config.out_pcm = optarg;
        //     break;
        case 'r':
            config.rate = atoi(optarg);
            break;
        case 's':
            save_audio = 1;
            break;
        case '?':
            printf("\n");
            printf(usage, argv[0]);
            exit(1);
        default:
            break;
        }
    }

    if (config.rec_channels <= 0) {
        printf("Input channels is not set, use '-c' to set one\n");
        exit(-1);
    }

    if (loopback_channel < 0 || loopback_channel >= config.rec_channels) {
        printf("The loopback channel %d is not valid\n", loopback_channel);
        exit(-1);
    }

    char *mic_channel_str = strtok(mic_list_str, ",");
    config.out_channels = 0;
    while (mic_channel_str != NULL) {
        int channel = atoi(mic_channel_str);
        if (channel >= config.rec_channels) {
            printf("The channel number %d must be less than input channels %d\n", channel, config.rec_channels);
            exit(-1);
        }

        mic_list[config.out_channels] = channel;
        config.out_channels++;

        if (config.out_channels >= config.rec_channels) {
            printf("The output channels %d must be less than input channels %d\n", config.out_channels, config.rec_channels);
            exit(-1);
        }

        mic_channel_str = strtok(NULL, ",");
    }

    if (daemon) {
        daemonize();
    }
    

    int frame_size = config.rate * 10 / 1000; // 10 ms

    if (save_audio)
    {
        fp_rec = fopen("/tmp/recording.raw", "wb");
        fp_out = fopen("/tmp/out.raw", "wb");

        if (fp_rec == NULL || fp_out == NULL)
        {
            printf("Fail to open file(s)\n");
            exit(1);
        }
    }

    rec = (int16_t *)calloc(frame_size * config.rec_channels, sizeof(int16_t));
    near = (int16_t *)calloc(frame_size * config.out_channels, sizeof(int16_t));
    far = (int16_t *)calloc(frame_size * config.ref_channels, sizeof(int16_t));
    out = (int16_t *)calloc(frame_size * config.out_channels, sizeof(int16_t));

    if (rec == NULL || near == NULL || far == NULL || out == NULL)
    {
        printf("Fail to allocate memory\n");
        exit(1);
    }

    // Configures signal handling.
    struct sigaction sig_int_handler;
    sig_int_handler.sa_handler = int_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;
    sigaction(SIGINT, &sig_int_handler, NULL);

    echo_state = speex_echo_state_init_mc(frame_size,
                                          config.filter_length,
                                          config.out_channels,
                                          config.ref_channels);
    speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &(config.rate));

    capture_start(&config);
    fifo_setup(&config);

    printf("Running... Press Ctrl+C to exit\n");

    int timeout = 200 * 1000 * frame_size / config.rate;    // ms


    while (!g_is_quit)
    {
        capture_read(rec, frame_size, timeout);

        for (int i=0; i<frame_size; i++) {
            for (int mic=0; mic<config.out_channels; mic++) {
                int channel = mic_list[mic];
                near[config.out_channels * i + channel] = rec[config.rec_channels * i + channel];
            }

            far[i] = rec[config.rec_channels * i + loopback_channel];
        }

        speex_echo_cancellation(echo_state, near, far, out);

        if (fp_rec)
        {
            fwrite(rec, 2, frame_size * config.rec_channels, fp_rec);
            fwrite(out, 2, frame_size * config.out_channels, fp_out);
        }

        fifo_write(out, frame_size);
    }

    if (fp_rec)
    {
        fclose(fp_rec);
        fclose(fp_out);
    }

    free(rec);
    free(near);
    free(far);
    free(out);

    capture_stop();

    exit(0);

    return 0;
}
