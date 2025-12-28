#include "dma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>

#define DMA_BASE_ADDR 0x3F007000
#define DMA_CHANNEL_SIZE 0x100
#define PAGE_SIZE 4096

typedef struct {
    uint32_t ti;
    uint32_t source_ad;
    uint32_t dest_ad;
    uint32_t txfr_len;
    uint32_t stride;
    uint32_t nextconbk;
    uint32_t reserved[2];
} dma_cb_t;

struct dma_context {
    dma_config_t config;
    int mem_fd;
    volatile uint32_t *dma_reg;
    
    void **buffers;
    dma_cb_t *control_blocks;
    void *control_blocks_phys;
    
    int current_buffer;
    bool running;
    pthread_t dma_thread;
    pthread_mutex_t mutex;
    
    dma_callback_t callback;
    void *callback_user_data;
};

static uint32_t mem_virt_to_phys(void *virt_addr) {
    return ((uint32_t)virt_addr) + 0x40000000;
}

static void* dma_thread_func(void *arg) {
    dma_context_t *ctx = (dma_context_t*)arg;
    
    while (ctx->running) {
        uint32_t cs = ctx->dma_reg[0];
        
        if (cs & (1 << 0)) {
            pthread_mutex_lock(&ctx->mutex);
            
            if (ctx->callback) {
                void *buffer = ctx->buffers[ctx->current_buffer];
                ctx->callback(buffer, ctx->config.buffer_size, ctx->callback_user_data);
            }
            
            ctx->current_buffer = (ctx->current_buffer + 1) % ctx->config.num_buffers;
            
            ctx->dma_reg[0] = (1 << 0);
            
            pthread_mutex_unlock(&ctx->mutex);
        }
        
        if (cs & (1 << 2)) {
            fprintf(stderr, "DMA error detected\n");
            ctx->dma_reg[0] = (1 << 2);
        }
        
        usleep(100);
    }
    
    return NULL;
}

int dma_init(dma_context_t **ctx, const dma_config_t *config) {
    if (!ctx || !config) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    *ctx = calloc(1, sizeof(dma_context_t));
    if (!*ctx) {
        return MICARRAY_ERROR_MEMORY;
    }
    
    (*ctx)->config = *config;
    (*ctx)->running = false;
    (*ctx)->current_buffer = 0;
    
    if (pthread_mutex_init(&(*ctx)->mutex, NULL) != 0) {
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_INIT;
    }
    
    (*ctx)->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if ((*ctx)->mem_fd < 0) {
        fprintf(stderr, "Failed to open /dev/mem: %s\n", strerror(errno));
        pthread_mutex_destroy(&(*ctx)->mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_DMA;
    }
    
    uint32_t dma_addr = DMA_BASE_ADDR + (config->channel * DMA_CHANNEL_SIZE);
    (*ctx)->dma_reg = mmap(NULL, DMA_CHANNEL_SIZE, PROT_READ | PROT_WRITE, 
                          MAP_SHARED, (*ctx)->mem_fd, dma_addr);
    
    if ((*ctx)->dma_reg == MAP_FAILED) {
        fprintf(stderr, "Failed to map DMA registers: %s\n", strerror(errno));
        close((*ctx)->mem_fd);
        pthread_mutex_destroy(&(*ctx)->mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_DMA;
    }
    
    (*ctx)->buffers = calloc(config->num_buffers, sizeof(void*));
    if (!(*ctx)->buffers) {
        munmap((void*)(*ctx)->dma_reg, DMA_CHANNEL_SIZE);
        close((*ctx)->mem_fd);
        pthread_mutex_destroy(&(*ctx)->mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    for (int i = 0; i < config->num_buffers; i++) {
        (*ctx)->buffers[i] = aligned_alloc(PAGE_SIZE, config->buffer_size);
        if (!(*ctx)->buffers[i]) {
            for (int j = 0; j < i; j++) {
                free((*ctx)->buffers[j]);
            }
            free((*ctx)->buffers);
            munmap((void*)(*ctx)->dma_reg, DMA_CHANNEL_SIZE);
            close((*ctx)->mem_fd);
            pthread_mutex_destroy(&(*ctx)->mutex);
            free(*ctx);
            *ctx = NULL;
            return MICARRAY_ERROR_MEMORY;
        }
        memset((*ctx)->buffers[i], 0, config->buffer_size);
    }
    
    size_t cb_size = config->num_buffers * sizeof(dma_cb_t);
    (*ctx)->control_blocks = aligned_alloc(PAGE_SIZE, cb_size);
    if (!(*ctx)->control_blocks) {
        for (int i = 0; i < config->num_buffers; i++) {
            free((*ctx)->buffers[i]);
        }
        free((*ctx)->buffers);
        munmap((void*)(*ctx)->dma_reg, DMA_CHANNEL_SIZE);
        close((*ctx)->mem_fd);
        pthread_mutex_destroy(&(*ctx)->mutex);
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_MEMORY;
    }
    
    for (int i = 0; i < config->num_buffers; i++) {
        dma_cb_t *cb = &(*ctx)->control_blocks[i];
        cb->ti = (1 << 26) | (1 << 6) | (1 << 1);
        cb->source_ad = mem_virt_to_phys(config->src_addr);
        cb->dest_ad = mem_virt_to_phys((*ctx)->buffers[i]);
        cb->txfr_len = config->buffer_size;
        cb->stride = 0;
        
        if (config->cyclic) {
            int next_i = (i + 1) % config->num_buffers;
            cb->nextconbk = mem_virt_to_phys(&(*ctx)->control_blocks[next_i]);
        } else {
            cb->nextconbk = 0;
        }
    }
    
    return MICARRAY_SUCCESS;
}

int dma_start(dma_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (ctx->running) {
        return MICARRAY_SUCCESS;
    }
    
    ctx->dma_reg[0] = (1 << 31);
    usleep(1000);
    
    ctx->dma_reg[1] = mem_virt_to_phys(&ctx->control_blocks[0]);
    
    ctx->dma_reg[0] = (1 << 0);
    
    ctx->running = true;
    
    if (pthread_create(&ctx->dma_thread, NULL, dma_thread_func, ctx) != 0) {
        ctx->running = false;
        ctx->dma_reg[0] = (1 << 31);
        return MICARRAY_ERROR_INIT;
    }
    
    return MICARRAY_SUCCESS;
}

int dma_stop(dma_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (!ctx->running) {
        return MICARRAY_SUCCESS;
    }
    
    ctx->running = false;
    
    ctx->dma_reg[0] = (1 << 31);
    
    if (pthread_join(ctx->dma_thread, NULL) != 0) {
        fprintf(stderr, "Failed to join DMA thread\n");
        return MICARRAY_ERROR_INIT;
    }
    
    return MICARRAY_SUCCESS;
}

int dma_cleanup(dma_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    dma_stop(ctx);
    
    if (ctx->control_blocks) {
        free(ctx->control_blocks);
    }
    
    if (ctx->buffers) {
        for (int i = 0; i < ctx->config.num_buffers; i++) {
            if (ctx->buffers[i]) {
                free(ctx->buffers[i]);
            }
        }
        free(ctx->buffers);
    }
    
    if (ctx->dma_reg != MAP_FAILED) {
        munmap((void*)ctx->dma_reg, DMA_CHANNEL_SIZE);
    }
    
    if (ctx->mem_fd >= 0) {
        close(ctx->mem_fd);
    }
    
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
    
    return MICARRAY_SUCCESS;
}

int dma_set_callback(dma_context_t *ctx, dma_callback_t callback, void *user_data) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    ctx->callback = callback;
    ctx->callback_user_data = user_data;
    
    return MICARRAY_SUCCESS;
}

int dma_get_buffer(dma_context_t *ctx, void **buffer, size_t *size) {
    if (!ctx || !buffer || !size) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    *buffer = ctx->buffers[ctx->current_buffer];
    *size = ctx->config.buffer_size;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    return MICARRAY_SUCCESS;
}

int dma_release_buffer(dma_context_t *ctx, void *buffer) {
    if (!ctx || !buffer) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    return MICARRAY_SUCCESS;
}

bool dma_is_running(dma_context_t *ctx) {
    return ctx ? ctx->running : false;
}

int dma_get_status(dma_context_t *ctx) {
    if (!ctx) {
        return -1;
    }
    
    return (int)ctx->dma_reg[0];
}
