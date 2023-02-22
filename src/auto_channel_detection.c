#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "conf.h"

#include "auto_channel_detection.h"

extern int g_is_quit;

#define BIPNORM_TO_INT16_SLOPE 32767.5f
#define INT16_TO_BIPNORM_SLOPE 1.0f / BIPNORM_TO_INT16_SLOPE

typedef struct _env_chan_map
{
    unsigned channel;
    float val;
} env_chan_map;

float int16_to_bipNorm(int16_t input)
{
    float input_start = INT16_MIN;
    float output_start = -1.0f;
    return output_start + (INT16_TO_BIPNORM_SLOPE * (input - input_start));
}

int16_t bipNorm_to_int16(float input)
{
    float input_start = -1.0f;
    float output_start = INT16_MIN;
    return output_start + roundf(BIPNORM_TO_INT16_SLOPE * (input - input_start));
}

float fast_exp3(float x) { return (6 + x * (6 + x * (3 + x))) * 0.16666666f; }

void peak_envelope_follower(env_chan_map *env, float s, float release)
{
    if (s > env->val)
        env->val = s;
    else
        env->val = s + release * (env->val - s);
}

void sort_smallest_k(env_chan_map env[], int n, int k)
{
    // For each arr[i] find whether
    // it is a part of n-smallest
    // with insertion sort concept
    for (int i = k; i < n; ++i)
    {

        // find largest from first k-elements
        float max_var = env[k - 1].val;
        int pos = k - 1;

        for (int j = k - 2; j >= 0; j--)
        {
            if (env[j].val > max_var)
            {
                max_var = env[j].val;
                pos = j;
            }
        }

        // if largest is greater than arr[i]
        // shift all element one place left
        if (max_var > env[i].val)
        {

            int j = pos;
            while (j < k - 1)
            {
                env_chan_map tmp = env[j];
                env[j] = env[j + 1];
                env[j + 1] = tmp;
                j++;
            }

            // swap arr[k-1] and arr[i]
            env_chan_map tmp = env[k - 1];
            env[k - 1] = env[i];
            env[i] = tmp;
        }
    }
}

void init_envelopes(conf_t *config, env_chan_map *env)
{
    for (int channel = 0; channel < config->rec_channels; channel++)
    {
        env[channel].channel = channel;
        env[channel].val = 0;
    }
}

int detect_loopback_channels(conf_t *config, int16_t *buf, int *loopback_list, env_chan_map *env, int frame_size,
                             float release)
{
    int loopback_list_tmp[32];
    int res = 0;

    // We loop over envelopes as they get reordered by volume on every pass!
    for (int env_idx = 0; env_idx < config->rec_channels; env_idx++)
    {
        for (int i = 0; i < frame_size; ++i)
        {
            unsigned channel = env[env_idx].channel;
            unsigned pos = config->rec_channels * i + channel;
            float s = int16_to_bipNorm(buf[pos]);
            // int16_t s = buf[pos];
            peak_envelope_follower(&env[env_idx], fabs(s), release);
        }
    }

    sort_smallest_k(env, config->rec_channels, config->ref_channels);

    // printf("detected loopback channels:");
    for (int channel = 0; channel < config->ref_channels; channel++)
    {
        loopback_list_tmp[channel] = env[channel].channel;
        // printf(" %d", loopback_list_tmp[channel]);
    }
    // printf("\n");

    // selected channels might be in different order
    for (int channel = 0; channel < config->ref_channels; channel++)
    {
        int in_prev_list = 0;
        for (int i = 0; i < config->ref_channels; i++)
        {
            if (loopback_list[i] == loopback_list_tmp[channel])
            {
                in_prev_list = 1;
                break;
            }
        }

        if (!in_prev_list)
        {
            if (loopback_list[channel] != 32)
            {
                // printf("channel %d was not previously selected\n", loopback_list_tmp[channel]);
            }
            res++;
        }
    }

    if (res)
    {
        printf("Current loopback channel best candidates:");
        for (int channel = 0; channel < config->ref_channels; channel++)
        {
            loopback_list[channel] = loopback_list_tmp[channel];
            printf(" %d", loopback_list[channel]);
        }
        printf("\n");
    }

    return res;
}

void auto_channel_detection(conf_t *config, int16_t *rec, int *mic_list, int *loopback_list, int frame_size,
                            int timeout, int coherence_window_size_ms, int total_window_size_ms, int envelope_ms,
                            int save_audio)
{
    int coherence_window_size = config->rate * coherence_window_size_ms / 1000;
    int total_window_size = config->rate * total_window_size_ms / 1000;
    int coherence_window_size_r = coherence_window_size;
    int total_window_size_r = total_window_size;
    env_chan_map *env = malloc(sizeof(env_chan_map) * config->rec_channels);
    float env_frames = config->rate * (envelope_ms / 1000.0f);
    float release = fast_exp3(-2.0f / env_frames);
    FILE *fp_rec = NULL;

    if (save_audio)
    {
        fp_rec = fopen("/tmp/auto_channel_detection.raw", "wb");

        if (fp_rec == NULL)
        {
            printf("Fail to open file(s)\n");
            exit(1);
        }
    }

    init_envelopes(config, env);

    for (int speaker = 0; speaker < config->ref_channels; speaker++)
    {
        loopback_list[speaker] = 32;
    }

    printf("Detecting loopback channels...\n");

    while (!g_is_quit && coherence_window_size_r > 0)
    {
        capture_read(rec, frame_size, timeout);

        int detect_res = detect_loopback_channels(config, rec, loopback_list, env, frame_size, release);
        if (detect_res)
        {
            // we detected different channels. Let's start over
            coherence_window_size_r = coherence_window_size;
        }
        else
        {
            // detected channels are the same as before
            coherence_window_size_r -= frame_size;
        }
        total_window_size_r -= frame_size;

        if (fp_rec)
        {
            fwrite(rec, 2, frame_size * config->rec_channels, fp_rec);
        }

        if (total_window_size_r <= 0)
        {
            fprintf(stderr,
                    "Failed to detect loopback channels within time limit\n" //
                    "Please make sure no audio is playing and try again\n"   //
            );
            exit(1);
        }
    };

    printf("%10s|%10s\n", "channel", "volume");
    for (int i = 0; i < config->rec_channels; i++)
    {
        printf("%10u|%10f\n", env[i].channel, env[i].val);
    }

    printf("Detected channels:\n");
    printf(" rec:");
    for (int i = 0; i < config->ref_channels; i++)
    {
        printf(" %u", loopback_list[i]);
    }
    printf("\n");

    printf(" mic:");
    int mic_list_idx = 0;
    for (int channel = 0; channel < config->rec_channels; channel++)
    {
        int found = 0;
        for (int i = 0; i < config->ref_channels; i++)
        {
            if (channel == loopback_list[i])
            {
                found = 1;
                break;
            }
        }
        if (found)
        {
            continue;
        }
        mic_list[mic_list_idx] = channel;
        mic_list_idx++;
        printf(" %d", channel);
    }
    printf("\n");
}
