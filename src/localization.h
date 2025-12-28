#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include "libmicarray.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct localization_context localization_context_t;

typedef struct {
    float x;
    float y;
    float z;
} microphone_position_t;

typedef struct {
    int num_microphones;
    microphone_position_t *mic_positions;
    float mic_spacing;
    int sample_rate;
    float speed_of_sound;
    int correlation_window_size;
    float min_confidence_threshold;
} localization_config_t;

int localization_init(localization_context_t **ctx, const localization_config_t *config);
int localization_process(localization_context_t *ctx, int16_t **mic_data, size_t samples, sound_location_t *location);
int localization_cleanup(localization_context_t *ctx);

int localization_set_mic_positions(localization_context_t *ctx, const microphone_position_t *positions, int count);
int localization_calibrate(localization_context_t *ctx, int16_t **calibration_data, size_t samples);

#ifdef __cplusplus
}
#endif

#endif
