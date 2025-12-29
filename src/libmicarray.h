#ifndef LIBMICARRAY_H
#define LIBMICARRAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MICROPHONES 16
#define MAX_BUFFER_SIZE 8192
#define DEFAULT_SAMPLE_RATE 16000

typedef struct {
    int num_microphones;
    float mic_spacing;
    int i2s_bus;
    int dma_buffer_size;
    int sample_rate;
    bool noise_reduction_enable;
    float noise_threshold;
    char algorithm[64];
    char output_device[64];
    float volume;
    bool enable_serial_logging;
    char log_file[256];
    char log_level[16];
} micarray_config_t;

typedef struct {
    float x;
    float y;
    float z;
    float confidence;
} sound_location_t;

typedef struct {
    int16_t *buffer;
    size_t size;
    size_t capacity;
    int channels;
    int sample_rate;
} audio_buffer_t;

typedef struct micarray_context micarray_context_t;

int micarray_init(micarray_context_t **ctx, const char *config_file);
int micarray_start(micarray_context_t *ctx);
int micarray_stop(micarray_context_t *ctx);
int micarray_cleanup(micarray_context_t *ctx);

int micarray_get_location(micarray_context_t *ctx, sound_location_t *location);
int micarray_set_volume(micarray_context_t *ctx, float volume);

const char* micarray_get_version(void);
const char* micarray_get_error_string(int error_code);

#define MICARRAY_SUCCESS 0
#define MICARRAY_ERROR_INIT -1
#define MICARRAY_ERROR_CONFIG -2
#define MICARRAY_ERROR_I2S -3
#define MICARRAY_ERROR_DMA -4
#define MICARRAY_ERROR_AUDIO_OUTPUT -5
#define MICARRAY_ERROR_MEMORY -6
#define MICARRAY_ERROR_INVALID_PARAM -7

#ifdef __cplusplus
}
#endif

#endif
