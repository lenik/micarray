#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "../src/localization.h"

static void test_localization_init(void) {
    printf("Testing localization initialization...\n");
    
    localization_context_t *ctx = NULL;
    
    microphone_position_t mic_positions[4] = {
        {0.0f, 0.0f, 0.0f},
        {0.015f, 0.0f, 0.0f},
        {0.0f, 0.015f, 0.0f},
        {0.015f, 0.015f, 0.0f}
    };
    
    localization_config_t config = {
        .num_microphones = 4,
        .mic_positions = mic_positions,
        .mic_spacing = 0.015f,
        .sample_rate = 16000,
        .speed_of_sound = 343.0f,
        .correlation_window_size = 1024,
        .min_confidence_threshold = 0.3f
    };
    
    int result = localization_init(&ctx, &config);
    assert(result == MICARRAY_SUCCESS);
    assert(ctx != NULL);
    
    result = localization_cleanup(ctx);
    assert(result == MICARRAY_SUCCESS);
    
    printf("✓ Localization initialization test passed\n");
}

static void test_localization_invalid_params(void) {
    printf("Testing localization invalid parameters...\n");
    
    localization_context_t *ctx = NULL;
    
    // Test NULL context pointer
    int result = localization_init(NULL, NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    // Test NULL config
    result = localization_init(&ctx, NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    printf("✓ Localization invalid parameters test passed\n");
}

static void test_localization_processing(void) {
    printf("Testing localization processing...\n");
    
    localization_context_t *ctx = NULL;
    
    microphone_position_t mic_positions[4] = {
        {-0.015f, -0.015f, 0.0f},
        {0.015f, -0.015f, 0.0f},
        {0.015f, 0.015f, 0.0f},
        {-0.015f, 0.015f, 0.0f}
    };
    
    localization_config_t config = {
        .num_microphones = 4,
        .mic_positions = mic_positions,
        .mic_spacing = 0.015f,
        .sample_rate = 16000,
        .speed_of_sound = 343.0f,
        .correlation_window_size = 1024,
        .min_confidence_threshold = 0.1f
    };
    
    int result = localization_init(&ctx, &config);
    assert(result == MICARRAY_SUCCESS);
    
    // Create test microphone data
    const size_t samples = 2048;
    int16_t **mic_data = malloc(4 * sizeof(int16_t*));
    for (int i = 0; i < 4; i++) {
        mic_data[i] = malloc(samples * sizeof(int16_t));
    }
    
    // Generate test signal with simulated delay
    // Source at (1.0, 0.0, 0.0) - to the right
    float source_x = 1.0f, source_y = 0.0f;
    
    for (size_t s = 0; s < samples; s++) {
        float signal = sinf(2.0f * M_PI * 1000.0f * s / 16000.0f) * 16384.0f;
        
        for (int mic = 0; mic < 4; mic++) {
            // Calculate distance from source to microphone
            float dx = source_x - mic_positions[mic].x;
            float dy = source_y - mic_positions[mic].y;
            float distance = sqrtf(dx*dx + dy*dy);
            
            // Calculate delay in samples
            int delay_samples = (int)(distance / 343.0f * 16000.0f);
            
            if (s >= delay_samples) {
                mic_data[mic][s] = (int16_t)signal;
            } else {
                mic_data[mic][s] = 0;
            }
        }
    }
    
    sound_location_t location;
    result = localization_process(ctx, mic_data, samples, &location);
    assert(result == MICARRAY_SUCCESS);
    
    // Check that we got some reasonable location estimate
    // (exact values depend on algorithm implementation)
    assert(location.confidence >= 0.0f && location.confidence <= 1.0f);
    
    // Clean up
    for (int i = 0; i < 4; i++) {
        free(mic_data[i]);
    }
    free(mic_data);
    
    result = localization_cleanup(ctx);
    assert(result == MICARRAY_SUCCESS);
    
    printf("✓ Localization processing test passed\n");
}

static void test_localization_mic_positions(void) {
    printf("Testing localization microphone position setting...\n");
    
    localization_context_t *ctx = NULL;
    
    microphone_position_t mic_positions[3] = {
        {0.0f, 0.0f, 0.0f},
        {0.01f, 0.0f, 0.0f},
        {0.0f, 0.01f, 0.0f}
    };
    
    localization_config_t config = {
        .num_microphones = 3,
        .mic_positions = mic_positions,
        .mic_spacing = 0.01f,
        .sample_rate = 16000,
        .speed_of_sound = 343.0f,
        .correlation_window_size = 1024,
        .min_confidence_threshold = 0.3f
    };
    
    int result = localization_init(&ctx, &config);
    assert(result == MICARRAY_SUCCESS);
    
    // Test setting new positions
    microphone_position_t new_positions[3] = {
        {0.0f, 0.0f, 0.0f},
        {0.02f, 0.0f, 0.0f},
        {0.0f, 0.02f, 0.0f}
    };
    
    result = localization_set_mic_positions(ctx, new_positions, 3);
    assert(result == MICARRAY_SUCCESS);
    
    // Test invalid parameters
    result = localization_set_mic_positions(NULL, new_positions, 3);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    result = localization_set_mic_positions(ctx, NULL, 3);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    result = localization_set_mic_positions(ctx, new_positions, 2); // Wrong count
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    result = localization_cleanup(ctx);
    assert(result == MICARRAY_SUCCESS);
    
    printf("✓ Localization microphone position setting test passed\n");
}

int main(void) {
    printf("Running localization module tests...\n\n");
    
    test_localization_init();
    test_localization_invalid_params();
    test_localization_processing();
    test_localization_mic_positions();
    
    printf("\n✅ All localization tests passed!\n");
    return 0;
}
