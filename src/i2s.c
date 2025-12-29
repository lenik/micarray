#define _GNU_SOURCE
#include "i2s.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <pthread.h>
#include <errno.h>

struct i2s_context {
    i2s_config_t config;
    int device_fd;
    bool running;
    pthread_t read_thread;
    pthread_mutex_t mutex;
    
    int16_t *ring_buffer;
    size_t ring_buffer_size;
    size_t write_pos;
    size_t read_pos;
    size_t available_samples;
    
    void (*callback)(int16_t *data, size_t samples, void *user_data);
    void *callback_user_data;
};

static void* i2s_read_thread(void *arg) {
    i2s_context_t *ctx = (i2s_context_t*)arg;
    int16_t *temp_buffer = malloc(ctx->config.buffer_size * sizeof(int16_t));
    
    if (!temp_buffer) {
        fprintf(stderr, "Failed to allocate I2S read buffer\n");
        return NULL;
    }
    
    while (ctx->running) {
        ssize_t bytes_read = read(ctx->device_fd, temp_buffer, 
                                ctx->config.buffer_size * sizeof(int16_t));
        
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            fprintf(stderr, "I2S read error: %s\n", strerror(errno));
            break;
        }
        
        size_t samples_read = bytes_read / sizeof(int16_t);
        
        pthread_mutex_lock(&ctx->mutex);
        
        for (size_t i = 0; i < samples_read && ctx->available_samples < ctx->ring_buffer_size; i++) {
            ctx->ring_buffer[ctx->write_pos] = temp_buffer[i];
            ctx->write_pos = (ctx->write_pos + 1) % ctx->ring_buffer_size;
            ctx->available_samples++;
        }
        
        pthread_mutex_unlock(&ctx->mutex);
        
        if (ctx->callback && samples_read > 0) {
            ctx->callback(temp_buffer, samples_read, ctx->callback_user_data);
        }
        
        usleep(100);
    }
    
    free(temp_buffer);
    return NULL;
}

int i2s_init(i2s_context_t **ctx, const i2s_config_t *config) {
    if (!ctx || !config) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    *ctx = calloc(1, sizeof(i2s_context_t));
    if (!*ctx) {
        return MICARRAY_ERROR_MEMORY;
    }
    
    (*ctx)->config = *config;
    (*ctx)->running = false;
    (*ctx)->device_fd = -1;
    
    if (pthread_mutex_init(&(*ctx)->mutex, NULL) != 0) {
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_INIT;
    }
    
    (*ctx)->ring_buffer_size = config->buffer_size * 4;
    (*ctx)->ring_buffer = calloc((*ctx)->ring_buffer_size, sizeof(int16_t));
    if (!(*ctx)->ring_buffer) {
        pthread_mutex_destroy(&(*ctx)->mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    char device_path[64];
    snprintf(device_path, sizeof(device_path), "/dev/spidev%d.0", config->bus_id);
    
    (*ctx)->device_fd = open(device_path, O_RDWR | O_NONBLOCK);
    if ((*ctx)->device_fd < 0) {
        fprintf(stderr, "Failed to open I2S device %s: %s\n", device_path, strerror(errno));
        free((*ctx)->ring_buffer);
        pthread_mutex_destroy(&(*ctx)->mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_I2S;
    }
    
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = config->bits_per_sample;
    uint32_t speed = config->sample_rate * config->channels * (config->bits_per_sample / 8);
    
    if (ioctl((*ctx)->device_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl((*ctx)->device_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl((*ctx)->device_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        fprintf(stderr, "Failed to configure I2S device: %s\n", strerror(errno));
        close((*ctx)->device_fd);
        free((*ctx)->ring_buffer);
        pthread_mutex_destroy(&(*ctx)->mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_I2S;
    }
    
    return MICARRAY_SUCCESS;
}

int i2s_start(i2s_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (ctx->running) {
        return MICARRAY_SUCCESS;
    }
    
    ctx->running = true;
    ctx->write_pos = 0;
    ctx->read_pos = 0;
    ctx->available_samples = 0;
    
    if (pthread_create(&ctx->read_thread, NULL, i2s_read_thread, ctx) != 0) {
        ctx->running = false;
        return MICARRAY_ERROR_INIT;
    }
    
    return MICARRAY_SUCCESS;
}

int i2s_stop(i2s_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->running) {
        return MICARRAY_SUCCESS;
    }
    
    ctx->running = false;
    
    if (pthread_join(ctx->read_thread, NULL) != 0) {
        fprintf(stderr, "Failed to join I2S read thread\n");
        return MICARRAY_ERROR_INIT;
    }
    
    return MICARRAY_SUCCESS;
}

int i2s_cleanup(i2s_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    i2s_stop(ctx);
    
    if (ctx->device_fd >= 0) {
        close(ctx->device_fd);
    }
    
    free(ctx->ring_buffer);
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
    
    return MICARRAY_SUCCESS;
}

int i2s_read_samples(i2s_context_t *ctx, int16_t *buffer, size_t samples) {
    if (!ctx || !buffer) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    size_t samples_to_read = (samples < ctx->available_samples) ? samples : ctx->available_samples;
    
    for (size_t i = 0; i < samples_to_read; i++) {
        buffer[i] = ctx->ring_buffer[ctx->read_pos];
        ctx->read_pos = (ctx->read_pos + 1) % ctx->ring_buffer_size;
        ctx->available_samples--;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    
    return (int)samples_to_read;
}

int i2s_set_callback(i2s_context_t *ctx, void (*callback)(int16_t *data, size_t samples, void *user_data), void *user_data) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    ctx->callback = callback;
    ctx->callback_user_data = user_data;
    
    return MICARRAY_SUCCESS;
}

bool i2s_is_running(i2s_context_t *ctx) {
    return ctx ? ctx->running : false;
}

int i2s_get_buffer_level(i2s_context_t *ctx) {
    if (!ctx) {
        return 0;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    int level = (int)ctx->available_samples;
    pthread_mutex_unlock(&ctx->mutex);
    
    return level;
}
