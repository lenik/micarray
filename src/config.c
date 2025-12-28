#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int parse_general_section(const char *key, const char *value, micarray_config_t *config) {
    if (strcmp(key, "log_level") == 0) {
        strncpy(config->log_level, value, sizeof(config->log_level) - 1);
        config->log_level[sizeof(config->log_level) - 1] = '\0';
        return 0;
    }
    return -1;
}

static int parse_microphone_section(const char *key, const char *value, micarray_config_t *config) {
    if (strcmp(key, "num_microphones") == 0) {
        config->num_microphones = atoi(value);
        return 0;
    } else if (strcmp(key, "mic_spacing") == 0) {
        char *endptr;
        config->mic_spacing = strtof(value, &endptr);
        if (*endptr == 'm' && *(endptr + 1) == 'm') {
            return 0;
        }
        return -1;
    } else if (strcmp(key, "i2s_bus") == 0) {
        config->i2s_bus = atoi(value);
        return 0;
    } else if (strcmp(key, "dma_buffer_size") == 0) {
        config->dma_buffer_size = atoi(value);
        return 0;
    } else if (strcmp(key, "sample_rate") == 0) {
        config->sample_rate = atoi(value);
        return 0;
    }
    return -1;
}

static int parse_noise_reduction_section(const char *key, const char *value, micarray_config_t *config) {
    if (strcmp(key, "enable") == 0) {
        config->noise_reduction_enable = (strcmp(value, "true") == 0);
        return 0;
    } else if (strcmp(key, "noise_threshold") == 0) {
        config->noise_threshold = strtof(value, NULL);
        return 0;
    } else if (strcmp(key, "algorithm") == 0) {
        strncpy(config->algorithm, value, sizeof(config->algorithm) - 1);
        config->algorithm[sizeof(config->algorithm) - 1] = '\0';
        return 0;
    }
    return -1;
}

static int parse_audio_output_section(const char *key, const char *value, micarray_config_t *config) {
    if (strcmp(key, "output_device") == 0) {
        strncpy(config->output_device, value, sizeof(config->output_device) - 1);
        config->output_device[sizeof(config->output_device) - 1] = '\0';
        return 0;
    } else if (strcmp(key, "volume") == 0) {
        config->volume = strtof(value, NULL);
        return 0;
    }
    return -1;
}

static int parse_logging_section(const char *key, const char *value, micarray_config_t *config) {
    if (strcmp(key, "enable_serial_logging") == 0) {
        config->enable_serial_logging = (strcmp(value, "true") == 0);
        return 0;
    } else if (strcmp(key, "log_file") == 0) {
        strncpy(config->log_file, value, sizeof(config->log_file) - 1);
        config->log_file[sizeof(config->log_file) - 1] = '\0';
        return 0;
    }
    return -1;
}

static char* trim_whitespace(char *str) {
    char *end;
    
    while (*str == ' ' || *str == '\t') str++;
    
    if (*str == 0) return str;
    
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    
    *(end + 1) = 0;
    
    return str;
}

static char* remove_quotes(char *str) {
    if (str[0] == '"' && str[strlen(str) - 1] == '"') {
        str[strlen(str) - 1] = '\0';
        return str + 1;
    }
    return str;
}

int config_parse_file(const char *filename, micarray_config_t *config) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening config file %s: %s\n", filename, strerror(errno));
        return MICARRAY_ERROR_CONFIG;
    }
    
    char line[256];
    char current_section[64] = "";
    int line_number = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        char *trimmed = trim_whitespace(line);
        
        if (strlen(trimmed) == 0 || trimmed[0] == '#') {
            continue;
        }
        
        if (trimmed[0] == '[' && trimmed[strlen(trimmed) - 1] == ']') {
            strncpy(current_section, trimmed + 1, strlen(trimmed) - 2);
            current_section[strlen(trimmed) - 2] = '\0';
            continue;
        }
        
        char *equals = strchr(trimmed, '=');
        if (!equals) {
            fprintf(stderr, "Invalid line %d in config file: %s\n", line_number, trimmed);
            continue;
        }
        
        *equals = '\0';
        char *key = trim_whitespace(trimmed);
        char *value = trim_whitespace(equals + 1);
        value = remove_quotes(value);
        
        int result = -1;
        if (strcmp(current_section, "General") == 0) {
            result = parse_general_section(key, value, config);
        } else if (strcmp(current_section, "MicrophoneArray") == 0) {
            result = parse_microphone_section(key, value, config);
        } else if (strcmp(current_section, "NoiseReduction") == 0) {
            result = parse_noise_reduction_section(key, value, config);
        } else if (strcmp(current_section, "AudioOutput") == 0) {
            result = parse_audio_output_section(key, value, config);
        } else if (strcmp(current_section, "Logging") == 0) {
            result = parse_logging_section(key, value, config);
        }
        
        if (result != 0) {
            fprintf(stderr, "Unknown key '%s' in section '%s' at line %d\n", key, current_section, line_number);
        }
    }
    
    fclose(file);
    return MICARRAY_SUCCESS;
}

int config_set_defaults(micarray_config_t *config) {
    if (!config) return MICARRAY_ERROR_INVALID_PARAM;
    
    config->num_microphones = 8;
    config->mic_spacing = 15.0f;
    config->i2s_bus = 1;
    config->dma_buffer_size = 1024;
    config->sample_rate = DEFAULT_SAMPLE_RATE;
    config->noise_reduction_enable = true;
    config->noise_threshold = 0.05f;
    strcpy(config->algorithm, "spectral_subtraction");
    strcpy(config->output_device, "headphones");
    config->volume = 0.8f;
    config->enable_serial_logging = true;
    strcpy(config->log_file, "/var/log/micarray.log");
    strcpy(config->log_level, "INFO");
    
    return MICARRAY_SUCCESS;
}

int config_validate(const micarray_config_t *config) {
    if (!config) return MICARRAY_ERROR_INVALID_PARAM;
    
    if (config->num_microphones < 1 || config->num_microphones > MAX_MICROPHONES) {
        fprintf(stderr, "Invalid number of microphones: %d (must be 1-%d)\n", 
                config->num_microphones, MAX_MICROPHONES);
        return MICARRAY_ERROR_CONFIG;
    }
    
    if (config->mic_spacing <= 0.0f) {
        fprintf(stderr, "Invalid microphone spacing: %f (must be > 0)\n", config->mic_spacing);
        return MICARRAY_ERROR_CONFIG;
    }
    
    if (config->dma_buffer_size <= 0 || config->dma_buffer_size > MAX_BUFFER_SIZE) {
        fprintf(stderr, "Invalid DMA buffer size: %d (must be 1-%d)\n", 
                config->dma_buffer_size, MAX_BUFFER_SIZE);
        return MICARRAY_ERROR_CONFIG;
    }
    
    if (config->sample_rate <= 0) {
        fprintf(stderr, "Invalid sample rate: %d (must be > 0)\n", config->sample_rate);
        return MICARRAY_ERROR_CONFIG;
    }
    
    if (config->volume < 0.0f || config->volume > 1.0f) {
        fprintf(stderr, "Invalid volume: %f (must be 0.0-1.0)\n", config->volume);
        return MICARRAY_ERROR_CONFIG;
    }
    
    return MICARRAY_SUCCESS;
}

void config_print(const micarray_config_t *config) {
    if (!config) return;
    
    printf("Configuration:\n");
    printf("  Log Level: %s\n", config->log_level);
    printf("  Microphones: %d\n", config->num_microphones);
    printf("  Mic Spacing: %.1fmm\n", config->mic_spacing);
    printf("  I2S Bus: %d\n", config->i2s_bus);
    printf("  DMA Buffer Size: %d\n", config->dma_buffer_size);
    printf("  Sample Rate: %d Hz\n", config->sample_rate);
    printf("  Noise Reduction: %s\n", config->noise_reduction_enable ? "enabled" : "disabled");
    printf("  Noise Threshold: %.3f\n", config->noise_threshold);
    printf("  Algorithm: %s\n", config->algorithm);
    printf("  Output Device: %s\n", config->output_device);
    printf("  Volume: %.1f\n", config->volume);
    printf("  Serial Logging: %s\n", config->enable_serial_logging ? "enabled" : "disabled");
    printf("  Log File: %s\n", config->log_file);
}
