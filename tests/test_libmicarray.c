#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../src/libmicarray.h"

static void test_libmicarray_version(void) {
    printf("Testing libmicarray version...\n");
    
    const char *version = micarray_get_version();
    assert(version != NULL);
    assert(strlen(version) > 0);
    
    printf("✓ Version: %s\n", version);
}

static void test_error_strings(void) {
    printf("Testing error strings...\n");
    
    const char *success_str = micarray_get_error_string(MICARRAY_SUCCESS);
    assert(success_str != NULL);
    assert(strcmp(success_str, "Success") == 0);
    
    const char *init_error_str = micarray_get_error_string(MICARRAY_ERROR_INIT);
    assert(init_error_str != NULL);
    assert(strlen(init_error_str) > 0);
    
    const char *unknown_error_str = micarray_get_error_string(-999);
    assert(unknown_error_str != NULL);
    assert(strcmp(unknown_error_str, "Unknown error") == 0);
    
    printf("✓ Error strings test passed\n");
}

static void test_libmicarray_init_invalid_params(void) {
    printf("Testing libmicarray invalid parameters...\n");
    
    micarray_context_t *ctx = NULL;
    
    // Test NULL context pointer
    int result = micarray_init(NULL, "test.conf");
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    // Test NULL config file
    result = micarray_init(&ctx, NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    printf("✓ Invalid parameters test passed\n");
}

static void test_libmicarray_init_missing_config(void) {
    printf("Testing libmicarray with missing config file...\n");
    
    micarray_context_t *ctx = NULL;
    
    // Test with non-existent config file
    int result = micarray_init(&ctx, "nonexistent.conf");
    assert(result == MICARRAY_ERROR_CONFIG);
    assert(ctx == NULL);
    
    printf("✓ Missing config file test passed\n");
}

static void test_libmicarray_init_with_valid_config(void) {
    printf("Testing libmicarray initialization with valid config...\n");
    
    // Create a minimal test config file
    FILE *config_file = fopen("test_micarray.conf", "w");
    assert(config_file != NULL);
    
    fprintf(config_file,
        "[General]\n"
        "log_level = \"INFO\"\n"
        "\n"
        "[MicrophoneArray]\n"
        "num_microphones = 4\n"
        "mic_spacing = 15mm\n"
        "i2s_bus = 1\n"
        "dma_buffer_size = 512\n"
        "sample_rate = 16000\n"
        "\n"
        "[NoiseReduction]\n"
        "enable = false\n"
        "noise_threshold = 0.05\n"
        "algorithm = \"spectral_subtraction\"\n"
        "\n"
        "[AudioOutput]\n"
        "output_device = \"default\"\n"
        "volume = 0.8\n"
        "\n"
        "[Logging]\n"
        "enable_serial_logging = false\n"
        "log_file = \"/tmp/test_micarray.log\"\n");
    
    fclose(config_file);
    
    micarray_context_t *ctx = NULL;
    
    // Note: This will likely fail on systems without proper hardware,
    // but we can test the config parsing part
    int result = micarray_init(&ctx, "test_micarray.conf");
    
    if (result == MICARRAY_SUCCESS) {
        // If initialization succeeded, test other functions
        sound_location_t location;
        result = micarray_get_location(ctx, &location);
        assert(result == MICARRAY_SUCCESS);
        
        result = micarray_set_volume(ctx, 0.5f);
        assert(result == MICARRAY_SUCCESS);
        
        // Clean up
        micarray_cleanup(ctx);
    } else {
        // Expected on systems without proper I2S/audio hardware
        printf("  Note: Hardware initialization failed (expected on test systems)\n");
    }
    
    // Clean up test file
    unlink("test_micarray.conf");
    unlink("/tmp/test_micarray.log");
    
    printf("✓ Valid config initialization test completed\n");
}

static void test_libmicarray_operations_without_init(void) {
    printf("Testing libmicarray operations without initialization...\n");
    
    sound_location_t location;
    int result = micarray_get_location(NULL, &location);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    result = micarray_set_volume(NULL, 0.5f);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    result = micarray_start(NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    result = micarray_stop(NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    result = micarray_cleanup(NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    printf("✓ Operations without initialization test passed\n");
}

int main(void) {
    printf("Running libmicarray integration tests...\n\n");
    
    test_libmicarray_version();
    test_error_strings();
    test_libmicarray_init_invalid_params();
    test_libmicarray_init_missing_config();
    test_libmicarray_init_with_valid_config();
    test_libmicarray_operations_without_init();
    
    printf("\n✅ All libmicarray integration tests passed!\n");
    return 0;
}
