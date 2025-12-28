#ifndef DMA_H
#define DMA_H

#include "libmicarray.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dma_context dma_context_t;

typedef struct {
    int channel;
    size_t buffer_size;
    int num_buffers;
    void *src_addr;
    void *dst_addr;
    bool cyclic;
} dma_config_t;

typedef void (*dma_callback_t)(void *buffer, size_t size, void *user_data);

int dma_init(dma_context_t **ctx, const dma_config_t *config);
int dma_start(dma_context_t *ctx);
int dma_stop(dma_context_t *ctx);
int dma_cleanup(dma_context_t *ctx);

int dma_set_callback(dma_context_t *ctx, dma_callback_t callback, void *user_data);
int dma_get_buffer(dma_context_t *ctx, void **buffer, size_t *size);
int dma_release_buffer(dma_context_t *ctx, void *buffer);

bool dma_is_running(dma_context_t *ctx);
int dma_get_status(dma_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
