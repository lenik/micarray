#ifndef I2S_H
#define I2S_H

#include "libmicarray.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct i2s_context i2s_context_t;

typedef struct {
    int bus_id;
    int sample_rate;
    int channels;
    int bits_per_sample;
    int buffer_size;
} i2s_config_t;

int i2s_init(i2s_context_t **ctx, const i2s_config_t *config);
int i2s_start(i2s_context_t *ctx);
int i2s_stop(i2s_context_t *ctx);
int i2s_cleanup(i2s_context_t *ctx);

int i2s_read_samples(i2s_context_t *ctx, int16_t *buffer, size_t samples);
int i2s_set_callback(i2s_context_t *ctx, void (*callback)(int16_t *data, size_t samples, void *user_data), void *user_data);

bool i2s_is_running(i2s_context_t *ctx);
int i2s_get_buffer_level(i2s_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
