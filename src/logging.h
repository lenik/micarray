#ifndef LOGGING_H
#define LOGGING_H

#include "libmicarray.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

typedef struct logging_context logging_context_t;

typedef struct {
    bool enable_serial_logging;
    bool enable_file_logging;
    char log_file[256];
    char serial_device[64];
    log_level_t log_level;
    int baud_rate;
} logging_config_t;

int logging_init(logging_context_t **ctx, const logging_config_t *config);
int logging_cleanup(logging_context_t *ctx);

void log_message(logging_context_t *ctx, log_level_t level, const char *format, ...);
void log_location_data(logging_context_t *ctx, const sound_location_t *location);
void log_noise_metrics(logging_context_t *ctx, float noise_before, float noise_after);
void log_audio_levels(logging_context_t *ctx, float *levels, int num_channels);

int logging_set_level(logging_context_t *ctx, log_level_t level);

#define LOG_DEBUG(ctx, ...) log_message(ctx, LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(ctx, ...) log_message(ctx, LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARN(ctx, ...) log_message(ctx, LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_ERROR(ctx, ...) log_message(ctx, LOG_LEVEL_ERROR, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
