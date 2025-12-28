#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../src/logging.h"

static void test_logging_init(void) {
    printf("Testing logging initialization...\n");
    
    logging_context_t *ctx = NULL;
    logging_config_t config = {
        .enable_serial_logging = false,
        .enable_file_logging = true,
        .log_level = LOG_LEVEL_INFO,
        .baud_rate = 115200
    };
    strcpy(config.log_file, "/tmp/test_micarray.log");
    strcpy(config.serial_device, "/dev/null");
    
    int result = logging_init(&ctx, &config);
    assert(result == MICARRAY_SUCCESS);
    assert(ctx != NULL);
    
    result = logging_cleanup(ctx);
    assert(result == MICARRAY_SUCCESS);
    
    printf("✓ Logging initialization test passed\n");
}

static void test_logging_invalid_params(void) {
    printf("Testing logging invalid parameters...\n");
    
    logging_context_t *ctx = NULL;
    
    // Test NULL context pointer
    int result = logging_init(NULL, NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    // Test NULL config
    result = logging_init(&ctx, NULL);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    printf("✓ Logging invalid parameters test passed\n");
}

static void test_logging_messages(void) {
    printf("Testing logging messages...\n");
    
    logging_context_t *ctx = NULL;
    logging_config_t config = {
        .enable_serial_logging = false,
        .enable_file_logging = true,
        .log_level = LOG_LEVEL_DEBUG,
        .baud_rate = 115200
    };
    strcpy(config.log_file, "/tmp/test_micarray_messages.log");
    strcpy(config.serial_device, "/dev/null");
    
    int result = logging_init(&ctx, &config);
    assert(result == MICARRAY_SUCCESS);
    
    // Test different log levels
    LOG_DEBUG(ctx, "Debug message: %d", 42);
    LOG_INFO(ctx, "Info message: %s", "test");
    LOG_WARN(ctx, "Warning message");
    LOG_ERROR(ctx, "Error message");
    
    // Test location logging
    sound_location_t location = {1.5f, 2.0f, 0.5f, 0.8f};
    log_location_data(ctx, &location);
    
    // Test noise metrics logging
    log_noise_metrics(ctx, 0.5f, 0.1f);
    
    // Test audio levels logging
    float levels[4] = {0.1f, 0.2f, 0.15f, 0.18f};
    log_audio_levels(ctx, levels, 4);
    
    result = logging_cleanup(ctx);
    assert(result == MICARRAY_SUCCESS);
    
    // Verify log file was created and has content
    FILE *log_file = fopen("/tmp/test_micarray_messages.log", "r");
    assert(log_file != NULL);
    
    char line[256];
    int line_count = 0;
    while (fgets(line, sizeof(line), log_file)) {
        line_count++;
    }
    fclose(log_file);
    
    assert(line_count > 0); // Should have logged some messages
    
    // Clean up
    unlink("/tmp/test_micarray_messages.log");
    
    printf("✓ Logging messages test passed\n");
}

static void test_logging_level_setting(void) {
    printf("Testing logging level setting...\n");
    
    logging_context_t *ctx = NULL;
    logging_config_t config = {
        .enable_serial_logging = false,
        .enable_file_logging = true,
        .log_level = LOG_LEVEL_INFO,
        .baud_rate = 115200
    };
    strcpy(config.log_file, "/tmp/test_micarray_level.log");
    strcpy(config.serial_device, "/dev/null");
    
    int result = logging_init(&ctx, &config);
    assert(result == MICARRAY_SUCCESS);
    
    // Test setting log level
    result = logging_set_level(ctx, LOG_LEVEL_ERROR);
    assert(result == MICARRAY_SUCCESS);
    
    // Test invalid context
    result = logging_set_level(NULL, LOG_LEVEL_ERROR);
    assert(result == MICARRAY_ERROR_INVALID_PARAM);
    
    result = logging_cleanup(ctx);
    assert(result == MICARRAY_SUCCESS);
    
    // Clean up
    unlink("/tmp/test_micarray_level.log");
    
    printf("✓ Logging level setting test passed\n");
}

int main(void) {
    printf("Running logging module tests...\n\n");
    
    test_logging_init();
    test_logging_invalid_params();
    test_logging_messages();
    test_logging_level_setting();
    
    printf("\n✅ All logging tests passed!\n");
    return 0;
}
