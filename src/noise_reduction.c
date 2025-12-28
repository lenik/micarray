#include "noise_reduction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>

#define PI 3.14159265358979323846

struct noise_reduction_context {
    noise_reduction_config_t config;
    
    float *window;
    float *input_buffer;
    float *output_buffer;
    float *overlap_buffer;
    
    fftwf_complex *fft_input;
    fftwf_complex *fft_output;
    fftwf_plan forward_plan;
    fftwf_plan inverse_plan;
    
    float *noise_spectrum;
    float *magnitude_spectrum;
    float *phase_spectrum;
    
    int buffer_pos;
    bool noise_profile_ready;
};

static void apply_hanning_window(float *data, int size) {
    for (int i = 0; i < size; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * PI * i / (size - 1)));
        data[i] *= w;
    }
}

static void spectral_subtraction(noise_reduction_context_t *ctx, fftwf_complex *spectrum, int size) {
    for (int i = 0; i < size / 2 + 1; i++) {
        float magnitude = sqrtf(spectrum[i][0] * spectrum[i][0] + spectrum[i][1] * spectrum[i][1]);
        float phase = atan2f(spectrum[i][1], spectrum[i][0]);
        
        ctx->magnitude_spectrum[i] = magnitude;
        ctx->phase_spectrum[i] = phase;
        
        if (ctx->noise_profile_ready) {
            float snr = magnitude / (ctx->noise_spectrum[i] + 1e-10f);
            float gain;
            
            if (snr > ctx->config.noise_threshold) {
                gain = 1.0f - ctx->config.alpha * (ctx->noise_spectrum[i] / magnitude);
            } else {
                gain = ctx->config.beta;
            }
            
            gain = fmaxf(gain, ctx->config.beta);
            gain = fminf(gain, 1.0f);
            
            magnitude *= gain;
        }
        
        spectrum[i][0] = magnitude * cosf(phase);
        spectrum[i][1] = magnitude * sinf(phase);
    }
}

int noise_reduction_init(noise_reduction_context_t **ctx, const noise_reduction_config_t *config) {
    if (!ctx || !config) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    *ctx = calloc(1, sizeof(noise_reduction_context_t));
    if (!*ctx) {
        return MICARRAY_ERROR_MEMORY;
    }
    
    (*ctx)->config = *config;
    (*ctx)->buffer_pos = 0;
    (*ctx)->noise_profile_ready = false;
    
    (*ctx)->window = malloc(config->frame_size * sizeof(float));
    (*ctx)->input_buffer = calloc(config->frame_size, sizeof(float));
    (*ctx)->output_buffer = calloc(config->frame_size, sizeof(float));
    (*ctx)->overlap_buffer = calloc(config->overlap, sizeof(float));
    
    (*ctx)->fft_input = fftwf_alloc_complex(config->frame_size);
    (*ctx)->fft_output = fftwf_alloc_complex(config->frame_size);
    
    (*ctx)->noise_spectrum = calloc(config->frame_size / 2 + 1, sizeof(float));
    (*ctx)->magnitude_spectrum = calloc(config->frame_size / 2 + 1, sizeof(float));
    (*ctx)->phase_spectrum = calloc(config->frame_size / 2 + 1, sizeof(float));
    
    if (!(*ctx)->window || !(*ctx)->input_buffer || !(*ctx)->output_buffer || 
        !(*ctx)->overlap_buffer || !(*ctx)->fft_input || !(*ctx)->fft_output ||
        !(*ctx)->noise_spectrum || !(*ctx)->magnitude_spectrum || !(*ctx)->phase_spectrum) {
        noise_reduction_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    for (int i = 0; i < config->frame_size; i++) {
        (*ctx)->window[i] = 0.5f * (1.0f - cosf(2.0f * PI * i / (config->frame_size - 1)));
    }
    
    (*ctx)->forward_plan = fftwf_plan_dft_r2c_1d(config->frame_size, 
                                                (float*)(*ctx)->fft_input, 
                                                (*ctx)->fft_output, 
                                                FFTW_MEASURE);
    
    (*ctx)->inverse_plan = fftwf_plan_dft_c2r_1d(config->frame_size, 
                                                (*ctx)->fft_output, 
                                                (float*)(*ctx)->fft_input, 
                                                FFTW_MEASURE);
    
    if (!(*ctx)->forward_plan || !(*ctx)->inverse_plan) {
        noise_reduction_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_INIT;
    }
    
    return MICARRAY_SUCCESS;
}

int noise_reduction_process(noise_reduction_context_t *ctx, int16_t *input, int16_t *output, size_t samples) {
    if (!ctx || !input || !output) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    size_t processed = 0;
    int hop_size = ctx->config.frame_size - ctx->config.overlap;
    
    while (processed < samples) {
        size_t to_copy = fminf(samples - processed, ctx->config.frame_size - ctx->buffer_pos);
        
        for (size_t i = 0; i < to_copy; i++) {
            ctx->input_buffer[ctx->buffer_pos + i] = input[processed + i] / 32768.0f;
        }
        
        ctx->buffer_pos += to_copy;
        processed += to_copy;
        
        if (ctx->buffer_pos >= ctx->config.frame_size) {
            memcpy(ctx->fft_input, ctx->input_buffer, ctx->config.frame_size * sizeof(float));
            apply_hanning_window((float*)ctx->fft_input, ctx->config.frame_size);
            
            fftwf_execute(ctx->forward_plan);
            
            if (strcmp(ctx->config.algorithm, "spectral_subtraction") == 0) {
                spectral_subtraction(ctx, ctx->fft_output, ctx->config.frame_size);
            }
            
            fftwf_execute(ctx->inverse_plan);
            
            for (int i = 0; i < ctx->config.frame_size; i++) {
                ((float*)ctx->fft_input)[i] /= ctx->config.frame_size;
            }
            
            apply_hanning_window((float*)ctx->fft_input, ctx->config.frame_size);
            
            for (int i = 0; i < ctx->config.overlap; i++) {
                ((float*)ctx->fft_input)[i] += ctx->overlap_buffer[i];
            }
            
            size_t output_samples = fminf(hop_size, samples - (processed - to_copy));
            for (size_t i = 0; i < output_samples; i++) {
                float sample = ((float*)ctx->fft_input)[i];
                sample = fmaxf(-1.0f, fminf(1.0f, sample));
                output[processed - to_copy + i] = (int16_t)(sample * 32767.0f);
            }
            
            memcpy(ctx->overlap_buffer, &((float*)ctx->fft_input)[hop_size], 
                   ctx->config.overlap * sizeof(float));
            
            memmove(ctx->input_buffer, &ctx->input_buffer[hop_size], 
                    (ctx->config.frame_size - hop_size) * sizeof(float));
            ctx->buffer_pos -= hop_size;
        }
    }
    
    return MICARRAY_SUCCESS;
}

int noise_reduction_update_noise_profile(noise_reduction_context_t *ctx, int16_t *noise_samples, size_t samples) {
    if (!ctx || !noise_samples) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    memset(ctx->noise_spectrum, 0, (ctx->config.frame_size / 2 + 1) * sizeof(float));
    
    int num_frames = 0;
    size_t processed = 0;
    
    while (processed + ctx->config.frame_size <= samples) {
        for (int i = 0; i < ctx->config.frame_size; i++) {
            ((float*)ctx->fft_input)[i] = noise_samples[processed + i] / 32768.0f;
        }
        
        apply_hanning_window((float*)ctx->fft_input, ctx->config.frame_size);
        fftwf_execute(ctx->forward_plan);
        
        for (int i = 0; i < ctx->config.frame_size / 2 + 1; i++) {
            float magnitude = sqrtf(ctx->fft_output[i][0] * ctx->fft_output[i][0] + 
                                  ctx->fft_output[i][1] * ctx->fft_output[i][1]);
            ctx->noise_spectrum[i] += magnitude;
        }
        
        num_frames++;
        processed += ctx->config.frame_size / 2;
    }
    
    if (num_frames > 0) {
        for (int i = 0; i < ctx->config.frame_size / 2 + 1; i++) {
            ctx->noise_spectrum[i] /= num_frames;
        }
        ctx->noise_profile_ready = true;
    }
    
    return MICARRAY_SUCCESS;
}

int noise_reduction_set_threshold(noise_reduction_context_t *ctx, float threshold) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    ctx->config.noise_threshold = threshold;
    return MICARRAY_SUCCESS;
}

int noise_reduction_cleanup(noise_reduction_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (ctx->forward_plan) fftwf_destroy_plan(ctx->forward_plan);
    if (ctx->inverse_plan) fftwf_destroy_plan(ctx->inverse_plan);
    
    if (ctx->fft_input) fftwf_free(ctx->fft_input);
    if (ctx->fft_output) fftwf_free(ctx->fft_output);
    
    free(ctx->window);
    free(ctx->input_buffer);
    free(ctx->output_buffer);
    free(ctx->overlap_buffer);
    free(ctx->noise_spectrum);
    free(ctx->magnitude_spectrum);
    free(ctx->phase_spectrum);
    
    free(ctx);
    
    return MICARRAY_SUCCESS;
}
