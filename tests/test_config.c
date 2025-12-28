#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../src/config.h"

static void test_config_defaults(void) {
    printf("Testing config defaults...\n");
    
    micarray_config_t config;
    int result = config_set_defaults(&config);
    
    assert(result == MICARRAY_SUCCESS);
    assert(config.num_microphones == 8);
    assert(config.mic_spacing == 15.0f);
    assert(config.i2s_bus == 1);
    assert(config.dma_buffer_size == 1024);
    assert(config.sample_rate == DEFAULT_SAMPLE_RATE);
    assert(config.noise_reduction_enable == true);
    assert(config.noise_threshold == 0.05f);
    assert(strcmp(config.algorithm, "spectral_subtraction") == 0);
    assert(strcmp(config.output_device, "headphones") == 0);
    assert(config.volume == 0.8f);
    assert(config.enable_serial_logging == true);
    
    printf("✓ Config defaults test passed\n");
}

static void test_config_validation(void) {
    printf("Testing config validation...\n");
    
    micarray_config_t config;
    config_set_defaults(&config);
    
    // Test valid config
    assert(config_validate(&config) == MICARRAY_SUCCESS);
    
    // Test invalid microphone count
    config.num_microphones = 0;
    assert(config_validate(&config) == MICARRAY_ERROR_CONFIG);
    
    config.num_microphones = MAX_MICROPHONES + 1;
    assert(config_validate(&config) == MICARRAY_ERROR_CONFIG);
    
    // Reset and test invalid mic spacing
    config_set_defaults(&config);
    config.mic_spacing = -1.0f;
    assert(config_validate(&config) == MICARRAY_ERROR_CONFIG);
    
    // Reset and test invalid buffer size
    config_set_defaults(&config);
    config.dma_buffer_size = 0;
    assert(config_validate(&config) == MICARRAY_ERROR_CONFIG);
    
    config.dma_buffer_size = MAX_BUFFER_SIZE + 1;
    assert(config_validate(&config) == MICARRAY_ERROR_CONFIG);
    
    // Reset and test invalid sample rate
    config_set_defaults(&config);
    config.sample_rate = -1;
    assert(config_validate(&config) == MICARRAY_ERROR_CONFIG);
    
    // Reset and test invalid volume
    config_set_defaults(&config);
    config.volume = -0.1f;
    assert(config_validate(&config) == MICARRAY_ERROR_CONFIG);
    
    config.volume = 1.1f;
    assert(config_validate(&config) == MICARRAY_ERROR_CONFIG);
    
    printf("✓ Config validation test passed\n");
}

static void test_config_file_parsing(void) {
    printf("Testing config file parsing...\n");
    
    // Create a test config file
    FILE *test_file = fopen("test_config.conf", "w");
    assert(test_file != NULL);
    
    fprintf(test_file, 
        "[General]\n"
        "log_level = \"DEBUG\"\n"
        "\n"
        "[MicrophoneArray]\n"
        "num_microphones = 6\n"
        "mic_spacing = 20mm\n"
        "i2s_bus = 2\n"
        "dma_buffer_size = 2048\n"
        "sample_rate = 48000\n"
        "\n"
        "[NoiseReduction]\n"
        "enable = false\n"
        "noise_threshold = 0.1\n"
        "algorithm = \"wiener_filter\"\n"
        "\n"
        "[AudioOutput]\n"
        "output_device = \"speakers\"\n"
        "volume = 0.5\n"
        "\n"
        "[Logging]\n"
        "enable_serial_logging = false\n"
        "log_file = \"/tmp/test.log\"\n");
    
    fclose(test_file);
    
    micarray_config_t config;
    config_set_defaults(&config);
    
    int result = config_parse_file("test_config.conf", &config);
    assert(result == MICARRAY_SUCCESS);
    
    assert(strcmp(config.log_level, "DEBUG") == 0);
    assert(config.num_microphones == 6);
    assert(config.mic_spacing == 20.0f);
    assert(config.i2s_bus == 2);
    assert(config.dma_buffer_size == 2048);
    assert(config.sample_rate == 48000);
    assert(config.noise_reduction_enable == false);
    assert(config.noise_threshold == 0.1f);
    assert(strcmp(config.algorithm, "wiener_filter") == 0);
    assert(strcmp(config.output_device, "speakers") == 0);
    assert(config.volume == 0.5f);
    assert(config.enable_serial_logging == false);
    assert(strcmp(config.log_file, "/tmp/test.log") == 0);
    
    // Clean up
    unlink("test_config.conf");
    
    printf("✓ Config file parsing test passed\n");
}

static void test_config_invalid_file(void) {
    printf("Testing invalid config file handling...\n");
    
    micarray_config_t config;
    
    // Test non-existent file
    int result = config_parse_file("nonexistent.conf", &config);
    assert(result == MICARRAY_ERROR_CONFIG);
    
    printf("✓ Invalid config file test passed\n");
}

int main(void) {
    printf("Running config module tests...\n\n");
    
    test_config_defaults();
    test_config_validation();
    test_config_file_parsing();
    test_config_invalid_file();
    
    printf("\n✅ All config tests passed!\n");
    return 0;
}
