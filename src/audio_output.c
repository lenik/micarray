#include "audio_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <errno.h>

struct audio_output_context {
    audio_output_config_t config;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    
    bool running;
    pthread_mutex_t mutex;
    
    int16_t *output_buffer;
    size_t buffer_frames;
};

static void apply_stereo_panning(int16_t *mono_data, int16_t *left_out, int16_t *right_out, 
                                size_t samples, const sound_location_t *location) {
    float angle = atan2f(location->y, location->x);
    float distance = sqrtf(location->x * location->x + location->y * location->y);
    
    float pan = angle / M_PI;
    pan = fmaxf(-1.0f, fminf(1.0f, pan));
    
    float distance_attenuation = 1.0f / (1.0f + distance * 0.1f);
    distance_attenuation = fmaxf(0.1f, fminf(1.0f, distance_attenuation));
    
    float left_gain = (1.0f - pan) * 0.5f + 0.5f;
    float right_gain = (1.0f + pan) * 0.5f + 0.5f;
    
    left_gain *= distance_attenuation * location->confidence;
    right_gain *= distance_attenuation * location->confidence;
    
    for (size_t i = 0; i < samples; i++) {
        left_out[i] = (int16_t)(mono_data[i] * left_gain);
        right_out[i] = (int16_t)(mono_data[i] * right_gain);
    }
}

int audio_output_init(audio_output_context_t **ctx, const audio_output_config_t *config) {
    if (!ctx || !config) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    *ctx = calloc(1, sizeof(audio_output_context_t));
    if (!*ctx) {
        return MICARRAY_ERROR_MEMORY;
    }
    
    (*ctx)->config = *config;
    (*ctx)->running = false;
    
    if (pthread_mutex_init(&(*ctx)->mutex, NULL) != 0) {
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_INIT;
    }
    
    int err = snd_pcm_open(&(*ctx)->pcm_handle, config->device_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Failed to open audio device %s: %s\n", config->device_name, snd_strerror(err));
        pthread_mutex_destroy(&(*ctx)->mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    err = snd_pcm_hw_params_malloc(&(*ctx)->hw_params);
    if (err < 0) {
        fprintf(stderr, "Failed to allocate hardware parameters: %s\n", snd_strerror(err));
        snd_pcm_close((*ctx)->pcm_handle);
        pthread_mutex_destroy(&(*ctx)->mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    err = snd_pcm_hw_params_any((*ctx)->pcm_handle, (*ctx)->hw_params);
    if (err < 0) {
        fprintf(stderr, "Failed to initialize hardware parameters: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    err = snd_pcm_hw_params_set_access((*ctx)->pcm_handle, (*ctx)->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        fprintf(stderr, "Failed to set access type: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    snd_pcm_format_t format = (config->bits_per_sample == 16) ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_S32_LE;
    err = snd_pcm_hw_params_set_format((*ctx)->pcm_handle, (*ctx)->hw_params, format);
    if (err < 0) {
        fprintf(stderr, "Failed to set sample format: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    unsigned int rate = config->sample_rate;
    err = snd_pcm_hw_params_set_rate_near((*ctx)->pcm_handle, (*ctx)->hw_params, &rate, 0);
    if (err < 0) {
        fprintf(stderr, "Failed to set sample rate: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    err = snd_pcm_hw_params_set_channels((*ctx)->pcm_handle, (*ctx)->hw_params, config->channels);
    if (err < 0) {
        fprintf(stderr, "Failed to set channel count: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    snd_pcm_uframes_t buffer_frames = config->buffer_size;
    err = snd_pcm_hw_params_set_buffer_size_near((*ctx)->pcm_handle, (*ctx)->hw_params, &buffer_frames);
    if (err < 0) {
        fprintf(stderr, "Failed to set buffer size: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    (*ctx)->buffer_frames = buffer_frames;
    
    err = snd_pcm_hw_params((*ctx)->pcm_handle, (*ctx)->hw_params);
    if (err < 0) {
        fprintf(stderr, "Failed to set hardware parameters: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    err = snd_pcm_sw_params_malloc(&(*ctx)->sw_params);
    if (err < 0) {
        fprintf(stderr, "Failed to allocate software parameters: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    err = snd_pcm_sw_params_current((*ctx)->pcm_handle, (*ctx)->sw_params);
    if (err < 0) {
        fprintf(stderr, "Failed to get current software parameters: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    err = snd_pcm_sw_params_set_start_threshold((*ctx)->pcm_handle, (*ctx)->sw_params, buffer_frames / 4);
    if (err < 0) {
        fprintf(stderr, "Failed to set start threshold: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    err = snd_pcm_sw_params((*ctx)->pcm_handle, (*ctx)->sw_params);
    if (err < 0) {
        fprintf(stderr, "Failed to set software parameters: %s\n", snd_strerror(err));
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    (*ctx)->output_buffer = malloc(buffer_frames * config->channels * sizeof(int16_t));
    if (!(*ctx)->output_buffer) {
        audio_output_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    return MICARRAY_SUCCESS;
}

int audio_output_start(audio_output_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (ctx->running) {
        return MICARRAY_SUCCESS;
    }
    
    int err = snd_pcm_prepare(ctx->pcm_handle);
    if (err < 0) {
        fprintf(stderr, "Failed to prepare audio device: %s\n", snd_strerror(err));
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    ctx->running = true;
    return MICARRAY_SUCCESS;
}

int audio_output_stop(audio_output_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->running) {
        return MICARRAY_SUCCESS;
    }
    
    ctx->running = false;
    
    int err = snd_pcm_drop(ctx->pcm_handle);
    if (err < 0) {
        fprintf(stderr, "Failed to stop audio device: %s\n", snd_strerror(err));
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    return MICARRAY_SUCCESS;
}

int audio_output_cleanup(audio_output_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    audio_output_stop(ctx);
    
    free(ctx->output_buffer);
    
    if (ctx->sw_params) {
        snd_pcm_sw_params_free(ctx->sw_params);
    }
    
    if (ctx->hw_params) {
        snd_pcm_hw_params_free(ctx->hw_params);
    }
    
    if (ctx->pcm_handle) {
        snd_pcm_close(ctx->pcm_handle);
    }
    
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
    
    return MICARRAY_SUCCESS;
}

int audio_output_write_stereo(audio_output_context_t *ctx, int16_t *left_channel, int16_t *right_channel, size_t samples) {
    if (!ctx || !left_channel || !right_channel) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->running) {
        return MICARRAY_ERROR_AUDIO_OUTPUT;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    for (size_t i = 0; i < samples; i++) {
        ctx->output_buffer[i * 2] = (int16_t)(left_channel[i] * ctx->config.volume);
        ctx->output_buffer[i * 2 + 1] = (int16_t)(right_channel[i] * ctx->config.volume);
    }
    
    snd_pcm_sframes_t frames_written = snd_pcm_writei(ctx->pcm_handle, ctx->output_buffer, samples);
    
    pthread_mutex_unlock(&ctx->mutex);
    
    if (frames_written < 0) {
        if (frames_written == -EPIPE) {
            fprintf(stderr, "Audio underrun occurred\n");
            snd_pcm_prepare(ctx->pcm_handle);
            return MICARRAY_SUCCESS;
        } else {
            fprintf(stderr, "Audio write error: %s\n", snd_strerror(frames_written));
            return MICARRAY_ERROR_AUDIO_OUTPUT;
        }
    }
    
    return MICARRAY_SUCCESS;
}

int audio_output_write_localized(audio_output_context_t *ctx, int16_t *audio_data, size_t samples, const sound_location_t *location) {
    if (!ctx || !audio_data || !location) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    int16_t *left_channel = malloc(samples * sizeof(int16_t));
    int16_t *right_channel = malloc(samples * sizeof(int16_t));
    
    if (!left_channel || !right_channel) {
        free(left_channel);
        free(right_channel);
        return MICARRAY_ERROR_MEMORY;
    }
    
    apply_stereo_panning(audio_data, left_channel, right_channel, samples, location);
    
    int result = audio_output_write_stereo(ctx, left_channel, right_channel, samples);
    
    free(left_channel);
    free(right_channel);
    
    return result;
}

int audio_output_set_volume(audio_output_context_t *ctx, float volume) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    ctx->config.volume = fmaxf(0.0f, fminf(1.0f, volume));
    pthread_mutex_unlock(&ctx->mutex);
    
    return MICARRAY_SUCCESS;
}

int audio_output_get_latency(audio_output_context_t *ctx) {
    if (!ctx) {
        return -1;
    }
    
    snd_pcm_sframes_t delay;
    int err = snd_pcm_delay(ctx->pcm_handle, &delay);
    if (err < 0) {
        return -1;
    }
    
    return (int)(delay * 1000 / ctx->config.sample_rate);
}

bool audio_output_is_running(audio_output_context_t *ctx) {
    return ctx ? ctx->running : false;
}
