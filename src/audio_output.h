#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include "libmicarray.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_output_context audio_output_context_t;

typedef struct {
    char device_name[64];
    int sample_rate;
    int channels;
    int bits_per_sample;
    int buffer_size;
    float volume;
} audio_output_config_t;

int audio_output_init(audio_output_context_t **ctx, const audio_output_config_t *config);
int audio_output_start(audio_output_context_t *ctx);
int audio_output_stop(audio_output_context_t *ctx);
int audio_output_cleanup(audio_output_context_t *ctx);

int audio_output_write_stereo(audio_output_context_t *ctx, int16_t *left_channel, int16_t *right_channel, size_t samples);
int audio_output_write_localized(audio_output_context_t *ctx, int16_t *audio_data, size_t samples, const sound_location_t *location);

int audio_output_set_volume(audio_output_context_t *ctx, float volume);
int audio_output_get_latency(audio_output_context_t *ctx);

bool audio_output_is_running(audio_output_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
