#include "libmicarray.h"
#include "config.h"
#include "i2s.h"
#include "dma.h"
#include "noise_reduction.h"
#include "localization.h"
#include "audio_output.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#define LIBMICARRAY_VERSION "1.0.0"

struct micarray_context {
    micarray_config_t config;
    
    i2s_context_t *i2s_ctx;
    dma_context_t *dma_ctx;
    noise_reduction_context_t *noise_ctx;
    localization_context_t *loc_ctx;
    audio_output_context_t *audio_ctx;
    logging_context_t *log_ctx;
    
    int16_t **mic_buffers;
    int16_t *processed_buffer;
    sound_location_t current_location;
    
    bool running;
    pthread_t processing_thread;
    pthread_mutex_t data_mutex;
};

static volatile bool g_shutdown_requested = false;

static void signal_handler(int sig) {
    g_shutdown_requested = true;
}

static void audio_callback(int16_t *data, size_t samples, void *user_data) {
    micarray_context_t *ctx = (micarray_context_t*)user_data;
    
    if (!ctx || !ctx->running) {
        return;
    }
    
    pthread_mutex_lock(&ctx->data_mutex);
    
    for (size_t i = 0; i < samples; i++) {
        int mic_idx = i % ctx->config.num_microphones;
        size_t sample_idx = i / ctx->config.num_microphones;
        
        if (sample_idx < samples / ctx->config.num_microphones) {
            ctx->mic_buffers[mic_idx][sample_idx] = data[i];
        }
    }
    
    pthread_mutex_unlock(&ctx->data_mutex);
}

static void* processing_thread_func(void *arg) {
    micarray_context_t *ctx = (micarray_context_t*)arg;
    const size_t buffer_size = ctx->config.dma_buffer_size;
    
    LOG_INFO(ctx->log_ctx, "Processing thread started");
    
    while (ctx->running && !g_shutdown_requested) {
        pthread_mutex_lock(&ctx->data_mutex);
        
        if (ctx->config.noise_reduction_enable && ctx->noise_ctx) {
            for (int i = 0; i < ctx->config.num_microphones; i++) {
                noise_reduction_process(ctx->noise_ctx, 
                                      ctx->mic_buffers[i], 
                                      ctx->mic_buffers[i], 
                                      buffer_size);
            }
        }
        
        if (ctx->loc_ctx) {
            localization_process(ctx->loc_ctx, 
                               ctx->mic_buffers, 
                               buffer_size, 
                               &ctx->current_location);
            
            log_location_data(ctx->log_ctx, &ctx->current_location);
        }
        
        memset(ctx->processed_buffer, 0, buffer_size * sizeof(int16_t));
        for (int i = 0; i < ctx->config.num_microphones; i++) {
            for (size_t j = 0; j < buffer_size; j++) {
                int32_t sum = ctx->processed_buffer[j] + ctx->mic_buffers[i][j];
                ctx->processed_buffer[j] = (int16_t)(sum / ctx->config.num_microphones);
            }
        }
        
        pthread_mutex_unlock(&ctx->data_mutex);
        
        if (ctx->audio_ctx) {
            audio_output_write_localized(ctx->audio_ctx, 
                                       ctx->processed_buffer, 
                                       buffer_size, 
                                       &ctx->current_location);
        }
        
        usleep(1000);
    }
    
    LOG_INFO(ctx->log_ctx, "Processing thread stopped");
    return NULL;
}

int micarray_init(micarray_context_t **ctx, const char *config_file) {
    if (!ctx || !config_file) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    *ctx = calloc(1, sizeof(micarray_context_t));
    if (!*ctx) {
        return MICARRAY_ERROR_MEMORY;
    }
    
    config_set_defaults(&(*ctx)->config);
    
    int result = config_parse_file(config_file, &(*ctx)->config);
    if (result != MICARRAY_SUCCESS) {
        free(*ctx);
        *ctx = NULL;
        return result;
    }
    
    result = config_validate(&(*ctx)->config);
    if (result != MICARRAY_SUCCESS) {
        free(*ctx);
        *ctx = NULL;
        return result;
    }
    
    if (pthread_mutex_init(&(*ctx)->data_mutex, NULL) != 0) {
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_INIT;
    }
    
    logging_config_t log_config = {
        .enable_serial_logging = (*ctx)->config.enable_serial_logging,
        .enable_file_logging = (strlen((*ctx)->config.log_file) > 0),
        .log_level = LOG_LEVEL_INFO,
        .baud_rate = 115200
    };
    strcpy(log_config.log_file, (*ctx)->config.log_file);
    strcpy(log_config.serial_device, "/dev/ttyUSB0");
    
    if (strcmp((*ctx)->config.log_level, "DEBUG") == 0) {
        log_config.log_level = LOG_LEVEL_DEBUG;
    } else if (strcmp((*ctx)->config.log_level, "WARN") == 0) {
        log_config.log_level = LOG_LEVEL_WARN;
    } else if (strcmp((*ctx)->config.log_level, "ERROR") == 0) {
        log_config.log_level = LOG_LEVEL_ERROR;
    }
    
    result = logging_init(&(*ctx)->log_ctx, &log_config);
    if (result != MICARRAY_SUCCESS) {
        pthread_mutex_destroy(&(*ctx)->data_mutex);
        free(*ctx);
        *ctx = NULL;
        return result;
    }
    
    LOG_INFO((*ctx)->log_ctx, "Initializing libmicarray v%s", LIBMICARRAY_VERSION);
    config_print(&(*ctx)->config);
    
    (*ctx)->mic_buffers = malloc((*ctx)->config.num_microphones * sizeof(int16_t*));
    if (!(*ctx)->mic_buffers) {
        logging_cleanup((*ctx)->log_ctx);
        pthread_mutex_destroy(&(*ctx)->data_mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    for (int i = 0; i < (*ctx)->config.num_microphones; i++) {
        (*ctx)->mic_buffers[i] = calloc((*ctx)->config.dma_buffer_size, sizeof(int16_t));
        if (!(*ctx)->mic_buffers[i]) {
            for (int j = 0; j < i; j++) {
                free((*ctx)->mic_buffers[j]);
            }
            free((*ctx)->mic_buffers);
            logging_cleanup((*ctx)->log_ctx);
            pthread_mutex_destroy(&(*ctx)->data_mutex);
            free(*ctx);
            *ctx = NULL;
            return MICARRAY_ERROR_MEMORY;
        }
    }
    
    (*ctx)->processed_buffer = calloc((*ctx)->config.dma_buffer_size, sizeof(int16_t));
    if (!(*ctx)->processed_buffer) {
        for (int i = 0; i < (*ctx)->config.num_microphones; i++) {
            free((*ctx)->mic_buffers[i]);
        }
        free((*ctx)->mic_buffers);
        logging_cleanup((*ctx)->log_ctx);
        pthread_mutex_destroy(&(*ctx)->data_mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    i2s_config_t i2s_config = {
        .bus_id = (*ctx)->config.i2s_bus,
        .sample_rate = (*ctx)->config.sample_rate,
        .channels = (*ctx)->config.num_microphones,
        .bits_per_sample = 16,
        .buffer_size = (*ctx)->config.dma_buffer_size
    };
    
    result = i2s_init(&(*ctx)->i2s_ctx, &i2s_config);
    if (result != MICARRAY_SUCCESS) {
        LOG_ERROR((*ctx)->log_ctx, "Failed to initialize I2S interface");
        micarray_cleanup(*ctx);
        *ctx = NULL;
        return result;
    }
    
    i2s_set_callback((*ctx)->i2s_ctx, audio_callback, *ctx);
    
    if ((*ctx)->config.noise_reduction_enable) {
        noise_reduction_config_t noise_config = {
            .noise_threshold = (*ctx)->config.noise_threshold,
            .frame_size = 1024,
            .overlap = 512,
            .alpha = 2.0f,
            .beta = 0.1f,
            .sample_rate = (*ctx)->config.sample_rate
        };
        strcpy(noise_config.algorithm, (*ctx)->config.algorithm);
        
        result = noise_reduction_init(&(*ctx)->noise_ctx, &noise_config);
        if (result != MICARRAY_SUCCESS) {
            LOG_ERROR((*ctx)->log_ctx, "Failed to initialize noise reduction");
            micarray_cleanup(*ctx);
            *ctx = NULL;
            return result;
        }
    }
    
    microphone_position_t *mic_positions = malloc((*ctx)->config.num_microphones * sizeof(microphone_position_t));
    if (!mic_positions) {
        micarray_cleanup(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    for (int i = 0; i < (*ctx)->config.num_microphones; i++) {
        float angle = 2.0f * M_PI * i / (*ctx)->config.num_microphones;
        mic_positions[i].x = (*ctx)->config.mic_spacing * cosf(angle) / 1000.0f;
        mic_positions[i].y = (*ctx)->config.mic_spacing * sinf(angle) / 1000.0f;
        mic_positions[i].z = 0.0f;
    }
    
    localization_config_t loc_config = {
        .num_microphones = (*ctx)->config.num_microphones,
        .mic_positions = mic_positions,
        .mic_spacing = (*ctx)->config.mic_spacing / 1000.0f,
        .sample_rate = (*ctx)->config.sample_rate,
        .speed_of_sound = 343.0f,
        .correlation_window_size = 1024,
        .min_confidence_threshold = 0.3f
    };
    
    result = localization_init(&(*ctx)->loc_ctx, &loc_config);
    free(mic_positions);
    if (result != MICARRAY_SUCCESS) {
        LOG_ERROR((*ctx)->log_ctx, "Failed to initialize localization");
        micarray_cleanup(*ctx);
        *ctx = NULL;
        return result;
    }
    
    audio_output_config_t audio_config = {
        .sample_rate = (*ctx)->config.sample_rate,
        .channels = 2,
        .bits_per_sample = 16,
        .buffer_size = (*ctx)->config.dma_buffer_size,
        .volume = (*ctx)->config.volume
    };
    strcpy(audio_config.device_name, "default");
    
    result = audio_output_init(&(*ctx)->audio_ctx, &audio_config);
    if (result != MICARRAY_SUCCESS) {
        LOG_ERROR((*ctx)->log_ctx, "Failed to initialize audio output");
        micarray_cleanup(*ctx);
        *ctx = NULL;
        return result;
    }
    
    (*ctx)->running = false;
    
    LOG_INFO((*ctx)->log_ctx, "libmicarray initialization complete");
    
    return MICARRAY_SUCCESS;
}

int micarray_start(micarray_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (ctx->running) {
        return MICARRAY_SUCCESS;
    }
    
    LOG_INFO(ctx->log_ctx, "Starting microphone array processing");
    
    int result = i2s_start(ctx->i2s_ctx);
    if (result != MICARRAY_SUCCESS) {
        LOG_ERROR(ctx->log_ctx, "Failed to start I2S interface");
        return result;
    }
    
    result = audio_output_start(ctx->audio_ctx);
    if (result != MICARRAY_SUCCESS) {
        LOG_ERROR(ctx->log_ctx, "Failed to start audio output");
        i2s_stop(ctx->i2s_ctx);
        return result;
    }
    
    ctx->running = true;
    
    if (pthread_create(&ctx->processing_thread, NULL, processing_thread_func, ctx) != 0) {
        ctx->running = false;
        audio_output_stop(ctx->audio_ctx);
        i2s_stop(ctx->i2s_ctx);
        LOG_ERROR(ctx->log_ctx, "Failed to create processing thread");
        return MICARRAY_ERROR_INIT;
    }
    
    LOG_INFO(ctx->log_ctx, "Microphone array processing started successfully");
    
    return MICARRAY_SUCCESS;
}

int micarray_stop(micarray_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->running) {
        return MICARRAY_SUCCESS;
    }
    
    LOG_INFO(ctx->log_ctx, "Stopping microphone array processing");
    
    ctx->running = false;
    
    if (pthread_join(ctx->processing_thread, NULL) != 0) {
        LOG_ERROR(ctx->log_ctx, "Failed to join processing thread");
    }
    
    audio_output_stop(ctx->audio_ctx);
    i2s_stop(ctx->i2s_ctx);
    
    LOG_INFO(ctx->log_ctx, "Microphone array processing stopped");
    
    return MICARRAY_SUCCESS;
}

int micarray_cleanup(micarray_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    micarray_stop(ctx);
    
    if (ctx->audio_ctx) {
        audio_output_cleanup(ctx->audio_ctx);
    }
    
    if (ctx->loc_ctx) {
        localization_cleanup(ctx->loc_ctx);
    }
    
    if (ctx->noise_ctx) {
        noise_reduction_cleanup(ctx->noise_ctx);
    }
    
    if (ctx->i2s_ctx) {
        i2s_cleanup(ctx->i2s_ctx);
    }
    
    if (ctx->dma_ctx) {
        dma_cleanup(ctx->dma_ctx);
    }
    
    if (ctx->mic_buffers) {
        for (int i = 0; i < ctx->config.num_microphones; i++) {
            free(ctx->mic_buffers[i]);
        }
        free(ctx->mic_buffers);
    }
    
    free(ctx->processed_buffer);
    
    if (ctx->log_ctx) {
        LOG_INFO(ctx->log_ctx, "libmicarray cleanup complete");
        logging_cleanup(ctx->log_ctx);
    }
    
    pthread_mutex_destroy(&ctx->data_mutex);
    free(ctx);
    
    return MICARRAY_SUCCESS;
}

int micarray_get_location(micarray_context_t *ctx, sound_location_t *location) {
    if (!ctx || !location) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ctx->data_mutex);
    *location = ctx->current_location;
    pthread_mutex_unlock(&ctx->data_mutex);
    
    return MICARRAY_SUCCESS;
}

int micarray_set_volume(micarray_context_t *ctx, float volume) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    ctx->config.volume = volume;
    
    if (ctx->audio_ctx) {
        return audio_output_set_volume(ctx->audio_ctx, volume);
    }
    
    return MICARRAY_SUCCESS;
}

const char* micarray_get_version(void) {
    return LIBMICARRAY_VERSION;
}

const char* micarray_get_error_string(int error_code) {
    switch (error_code) {
        case MICARRAY_SUCCESS:
            return "Success";
        case MICARRAY_ERROR_INIT:
            return "Initialization error";
        case MICARRAY_ERROR_CONFIG:
            return "Configuration error";
        case MICARRAY_ERROR_I2S:
            return "I2S interface error";
        case MICARRAY_ERROR_DMA:
            return "DMA error";
        case MICARRAY_ERROR_AUDIO_OUTPUT:
            return "Audio output error";
        case MICARRAY_ERROR_MEMORY:
            return "Memory allocation error";
        case MICARRAY_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        default:
            return "Unknown error";
    }
}
