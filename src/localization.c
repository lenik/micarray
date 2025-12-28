#include "localization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_DELAY_SAMPLES 1000
#define DEFAULT_SPEED_OF_SOUND 343.0f

struct localization_context {
    localization_config_t config;
    microphone_position_t *mic_positions;
    
    float **correlation_buffers;
    float *delay_estimates;
    float *confidence_values;
    
    int16_t **mic_buffers;
    size_t buffer_size;
};

static float cross_correlate(int16_t *sig1, int16_t *sig2, size_t len, int delay) {
    float correlation = 0.0f;
    float norm1 = 0.0f, norm2 = 0.0f;
    
    for (size_t i = 0; i < len - abs(delay); i++) {
        int idx1 = (delay >= 0) ? i : i - delay;
        int idx2 = (delay >= 0) ? i + delay : i;
        
        if (idx1 >= 0 && idx1 < len && idx2 >= 0 && idx2 < len) {
            float s1 = sig1[idx1] / 32768.0f;
            float s2 = sig2[idx2] / 32768.0f;
            
            correlation += s1 * s2;
            norm1 += s1 * s1;
            norm2 += s2 * s2;
        }
    }
    
    float denominator = sqrtf(norm1 * norm2);
    return (denominator > 0.0f) ? correlation / denominator : 0.0f;
}

static float estimate_delay(int16_t *ref_signal, int16_t *target_signal, size_t samples, int max_delay) {
    float max_correlation = -1.0f;
    float best_delay = 0.0f;
    
    for (int delay = -max_delay; delay <= max_delay; delay++) {
        float correlation = cross_correlate(ref_signal, target_signal, samples, delay);
        
        if (correlation > max_correlation) {
            max_correlation = correlation;
            best_delay = (float)delay;
        }
    }
    
    return best_delay;
}

static void trilaterate_3d(localization_context_t *ctx, float *delays, sound_location_t *location) {
    float A[3][4];
    int num_equations = 0;
    
    for (int i = 1; i < ctx->config.num_microphones && num_equations < 3; i++) {
        float dx = ctx->mic_positions[i].x - ctx->mic_positions[0].x;
        float dy = ctx->mic_positions[i].y - ctx->mic_positions[0].y;
        float dz = ctx->mic_positions[i].z - ctx->mic_positions[0].z;
        
        float distance_diff = delays[i] * ctx->config.speed_of_sound;
        
        A[num_equations][0] = 2.0f * dx;
        A[num_equations][1] = 2.0f * dy;
        A[num_equations][2] = 2.0f * dz;
        A[num_equations][3] = distance_diff * distance_diff - 
                             (dx * dx + dy * dy + dz * dz);
        
        num_equations++;
    }
    
    if (num_equations < 3) {
        location->x = 0.0f;
        location->y = 0.0f;
        location->z = 0.0f;
        location->confidence = 0.0f;
        return;
    }
    
    for (int i = 0; i < 3; i++) {
        int max_row = i;
        for (int j = i + 1; j < 3; j++) {
            if (fabsf(A[j][i]) > fabsf(A[max_row][i])) {
                max_row = j;
            }
        }
        
        if (max_row != i) {
            for (int k = 0; k < 4; k++) {
                float temp = A[i][k];
                A[i][k] = A[max_row][k];
                A[max_row][k] = temp;
            }
        }
        
        if (fabsf(A[i][i]) < 1e-10f) {
            location->x = 0.0f;
            location->y = 0.0f;
            location->z = 0.0f;
            location->confidence = 0.0f;
            return;
        }
        
        for (int j = i + 1; j < 3; j++) {
            float factor = A[j][i] / A[i][i];
            for (int k = i; k < 4; k++) {
                A[j][k] -= factor * A[i][k];
            }
        }
    }
    
    float x[3];
    for (int i = 2; i >= 0; i--) {
        x[i] = A[i][3];
        for (int j = i + 1; j < 3; j++) {
            x[i] -= A[i][j] * x[j];
        }
        x[i] /= A[i][i];
    }
    
    location->x = x[0];
    location->y = x[1];
    location->z = x[2];
    
    float avg_confidence = 0.0f;
    for (int i = 0; i < ctx->config.num_microphones; i++) {
        avg_confidence += ctx->confidence_values[i];
    }
    location->confidence = avg_confidence / ctx->config.num_microphones;
}

int localization_init(localization_context_t **ctx, const localization_config_t *config) {
    if (!ctx || !config) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    *ctx = calloc(1, sizeof(localization_context_t));
    if (!*ctx) {
        return MICARRAY_ERROR_MEMORY;
    }
    
    (*ctx)->config = *config;
    
    if ((*ctx)->config.speed_of_sound <= 0.0f) {
        (*ctx)->config.speed_of_sound = DEFAULT_SPEED_OF_SOUND;
    }
    
    (*ctx)->mic_positions = malloc(config->num_microphones * sizeof(microphone_position_t));
    (*ctx)->delay_estimates = malloc(config->num_microphones * sizeof(float));
    (*ctx)->confidence_values = malloc(config->num_microphones * sizeof(float));
    
    if (!(*ctx)->mic_positions || !(*ctx)->delay_estimates || !(*ctx)->confidence_values) {
        localization_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    if (config->mic_positions) {
        memcpy((*ctx)->mic_positions, config->mic_positions, 
               config->num_microphones * sizeof(microphone_position_t));
    } else {
        for (int i = 0; i < config->num_microphones; i++) {
            float angle = 2.0f * M_PI * i / config->num_microphones;
            (*ctx)->mic_positions[i].x = config->mic_spacing * cosf(angle);
            (*ctx)->mic_positions[i].y = config->mic_spacing * sinf(angle);
            (*ctx)->mic_positions[i].z = 0.0f;
        }
    }
    
    (*ctx)->correlation_buffers = malloc(config->num_microphones * sizeof(float*));
    if (!(*ctx)->correlation_buffers) {
        localization_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    for (int i = 0; i < config->num_microphones; i++) {
        (*ctx)->correlation_buffers[i] = malloc(config->correlation_window_size * sizeof(float));
        if (!(*ctx)->correlation_buffers[i]) {
            localization_cleanup(*ctx);
            *ctx = NULL;
            return MICARRAY_ERROR_MEMORY;
        }
    }
    
    return MICARRAY_SUCCESS;
}

int localization_process(localization_context_t *ctx, int16_t **mic_data, size_t samples, sound_location_t *location) {
    if (!ctx || !mic_data || !location) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (samples < ctx->config.correlation_window_size) {
        location->x = 0.0f;
        location->y = 0.0f;
        location->z = 0.0f;
        location->confidence = 0.0f;
        return MICARRAY_SUCCESS;
    }
    
    int max_delay = (int)(ctx->config.mic_spacing * 2.0f / ctx->config.speed_of_sound * ctx->config.sample_rate);
    max_delay = fminf(max_delay, MAX_DELAY_SAMPLES);
    
    int16_t *reference_mic = mic_data[0];
    
    for (int i = 0; i < ctx->config.num_microphones; i++) {
        if (i == 0) {
            ctx->delay_estimates[i] = 0.0f;
            ctx->confidence_values[i] = 1.0f;
        } else {
            ctx->delay_estimates[i] = estimate_delay(reference_mic, mic_data[i], samples, max_delay);
            
            float max_correlation = -1.0f;
            for (int delay = -max_delay; delay <= max_delay; delay++) {
                float correlation = cross_correlate(reference_mic, mic_data[i], samples, delay);
                if (correlation > max_correlation) {
                    max_correlation = correlation;
                }
            }
            ctx->confidence_values[i] = max_correlation;
        }
    }
    
    float avg_confidence = 0.0f;
    for (int i = 0; i < ctx->config.num_microphones; i++) {
        avg_confidence += ctx->confidence_values[i];
    }
    avg_confidence /= ctx->config.num_microphones;
    
    if (avg_confidence < ctx->config.min_confidence_threshold) {
        location->x = 0.0f;
        location->y = 0.0f;
        location->z = 0.0f;
        location->confidence = avg_confidence;
        return MICARRAY_SUCCESS;
    }
    
    for (int i = 0; i < ctx->config.num_microphones; i++) {
        ctx->delay_estimates[i] /= ctx->config.sample_rate;
    }
    
    trilaterate_3d(ctx, ctx->delay_estimates, location);
    
    return MICARRAY_SUCCESS;
}

int localization_set_mic_positions(localization_context_t *ctx, const microphone_position_t *positions, int count) {
    if (!ctx || !positions || count != ctx->config.num_microphones) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    memcpy(ctx->mic_positions, positions, count * sizeof(microphone_position_t));
    
    return MICARRAY_SUCCESS;
}

int localization_calibrate(localization_context_t *ctx, int16_t **calibration_data, size_t samples) {
    if (!ctx || !calibration_data) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    return MICARRAY_SUCCESS;
}

int localization_cleanup(localization_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (ctx->correlation_buffers) {
        for (int i = 0; i < ctx->config.num_microphones; i++) {
            free(ctx->correlation_buffers[i]);
        }
        free(ctx->correlation_buffers);
    }
    
    free(ctx->mic_positions);
    free(ctx->delay_estimates);
    free(ctx->confidence_values);
    free(ctx);
    
    return MICARRAY_SUCCESS;
}
