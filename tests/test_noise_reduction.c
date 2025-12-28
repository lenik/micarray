#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "../src/noise_reduction.h"

static void test_noise_reduction_init(void) {
    printf("Testing noise reduction initialization...\n");
    
    noise_reduction_context_t *ctx = NULL;
    noise_reduction_config_t config = {
        .noise_threshold = 0.05f,
        .frame_size = 1024,
        .overlap = 512,
        .alpha = 2.0f,
        .beta = 0.1f,
        .sample_rate = 16000
    };
    strcpy(config.algorithm, "spectral_subtraction");
    
    int result = noise_reduction_init(&ctx, &config);
    assert(result == MICARRAY_SUCCESS);
    assert(ctx != NULL);
    
    result = noise_reduction_cleanup(ctx);
    assert(result == MICARRAY_SUCCESS);
    
    printf("✓ Noise reduction initialization test passed\n");
}

static void test_noise_reduction_invalid_params(void) {
    printf("Testing noise reduction invalid parameters...\n");
    
    noise_reduction_context_t *ctx = NULL;
    
    // Test NULL context pointer
    int result = noise_reduction_init(NULL, NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    // Test NULL config
    result = noise_reduction_init(&ctx, NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    printf("✓ Noise reduction invalid parameters test passed\n");
}

static void test_noise_reduction_processing(void) {
    printf("Testing noise reduction processing...\n");
    
    noise_reduction_context_t *ctx = NULL;
    noise_reduction_config_t config = {
        .noise_threshold = 0.05f,
        .frame_size = 1024,
        .overlap = 512,
        .alpha = 2.0f,
        .beta = 0.1f,
        .sample_rate = 16000
    };
    strcpy(config.algorithm, "spectral_subtraction");
    
    int result = noise_reduction_init(&ctx, &config);
    assert(result == MICARRAY_SUCCESS);
    
    // Create test audio data (sine wave + noise)
    const size_t samples = 2048;
    int16_t *input = malloc(samples * sizeof(int16_t));
    int16_t *output = malloc(samples * sizeof(int16_t));
    int16_t *noise_profile = malloc(samples * sizeof(int16_t));
    
    assert(input != NULL && output != NULL && noise_profile != NULL);
    
    // Generate test signals
    for (size_t i = 0; i < samples; i++) {
        // Pure sine wave at 1kHz
        float signal = sinf(2.0f * M_PI * 1000.0f * i / 16000.0f);
        // Add some noise
        float noise = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        input[i] = (int16_t)((signal + noise) * 16384.0f);
        
        // Noise profile (just noise)
        noise_profile[i] = (int16_t)(noise * 16384.0f);
    }
    
    // Update noise profile
    result = noise_reduction_update_noise_profile(ctx, noise_profile, samples);
    assert(result == MICARRAY_SUCCESS);
    
    // Process audio
    result = noise_reduction_process(ctx, input, output, samples);
    assert(result == MICARRAY_SUCCESS);
    
    // Verify output is different from input (processing occurred)
    bool different = false;
    for (size_t i = 0; i < samples; i++) {
        if (abs(input[i] - output[i]) > 10) {
            different = true;
            break;
        }
    }
    assert(different);
    
    free(input);
    free(output);
    free(noise_profile);
    
    result = noise_reduction_cleanup(ctx);
    assert(result == MICARRAY_SUCCESS);
    
    printf("✓ Noise reduction processing test passed\n");
}

static void test_noise_reduction_threshold_setting(void) {
    printf("Testing noise reduction threshold setting...\n");
    
    noise_reduction_context_t *ctx = NULL;
    noise_reduction_config_t config = {
        .noise_threshold = 0.05f,
        .frame_size = 1024,
        .overlap = 512,
        .alpha = 2.0f,
        .beta = 0.1f,
        .sample_rate = 16000
    };
    strcpy(config.algorithm, "spectral_subtraction");
    
    int result = noise_reduction_init(&ctx, &config);
    assert(result == MICARRAY_SUCCESS);
    
    // Test setting threshold
    result = noise_reduction_set_threshold(ctx, 0.1f);
    assert(result == MICARRAY_SUCCESS);
    
    // Test invalid context
    result = noise_reduction_set_threshold(NULL, 0.1f);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    result = noise_reduction_cleanup(ctx);
    assert(result == MICARRAY_SUCCESS);
    
    printf("✓ Noise reduction threshold setting test passed\n");
}

int main(void) {
    printf("Running noise reduction module tests...\n\n");
    
    test_noise_reduction_init();
    test_noise_reduction_invalid_params();
    test_noise_reduction_processing();
    test_noise_reduction_threshold_setting();
    
    printf("\n✅ All noise reduction tests passed!\n");
    return 0;
}
