#ifndef NOISE_REDUCTION_H
#define NOISE_REDUCTION_H

#include "libmicarray.h"
#include <stdint.h>
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct noise_reduction_context noise_reduction_context_t;

typedef struct {
    char algorithm[64];
    float noise_threshold;
    int frame_size;
    int overlap;
    float alpha;
    float beta;
    int sample_rate;
} noise_reduction_config_t;

int noise_reduction_init(noise_reduction_context_t **ctx, const noise_reduction_config_t *config);
int noise_reduction_process(noise_reduction_context_t *ctx, int16_t *input, int16_t *output, size_t samples);
int noise_reduction_cleanup(noise_reduction_context_t *ctx);

int noise_reduction_update_noise_profile(noise_reduction_context_t *ctx, int16_t *noise_samples, size_t samples);
int noise_reduction_set_threshold(noise_reduction_context_t *ctx, float threshold);

#ifdef __cplusplus
}
#endif

#endif
