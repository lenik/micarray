#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <errno.h>

struct logging_context {
    logging_config_t config;
    FILE *log_file;
    int serial_fd;
    pthread_mutex_t mutex;
    bool initialized;
};

static const char* log_level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static int setup_serial_port(int fd, int baud_rate) {
    struct termios tty;
    
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "Error getting serial port attributes: %s\n", strerror(errno));
        return -1;
    }
    
    speed_t speed;
    switch (baud_rate) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default: speed = B9600; break;
    }
    
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;
    
    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "Error setting serial port attributes: %s\n", strerror(errno));
        return -1;
    }
    
    return 0;
}

int logging_init(logging_context_t **ctx, const logging_config_t *config) {
    if (!ctx || !config) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    *ctx = calloc(1, sizeof(logging_context_t));
    if (!*ctx) {
        return MICARRAY_ERROR_MEMORY;
    }
    
    (*ctx)->config = *config;
    (*ctx)->log_file = NULL;
    (*ctx)->serial_fd = -1;
    (*ctx)->initialized = false;
    
    if (pthread_mutex_init(&(*ctx)->mutex, NULL) != 0) {
        free(*ctx);
        *ctx = NULL;
        return MICARRAY_ERROR_INIT;
    }
    
    if (config->enable_file_logging && strlen(config->log_file) > 0) {
        (*ctx)->log_file = fopen(config->log_file, "a");
        if (!(*ctx)->log_file) {
            fprintf(stderr, "Failed to open log file %s: %s\n", config->log_file, strerror(errno));
        }
    }
    
    if (config->enable_serial_logging && strlen(config->serial_device) > 0) {
        (*ctx)->serial_fd = open(config->serial_device, O_WRONLY | O_NOCTTY | O_NDELAY);
        if ((*ctx)->serial_fd < 0) {
            fprintf(stderr, "Failed to open serial device %s: %s\n", config->serial_device, strerror(errno));
        } else {
            if (setup_serial_port((*ctx)->serial_fd, config->baud_rate) != 0) {
                close((*ctx)->serial_fd);
                (*ctx)->serial_fd = -1;
            }
        }
    }
    
    (*ctx)->initialized = true;
    
    LOG_INFO(*ctx, "Logging system initialized");
    
    return MICARRAY_SUCCESS;
}

int logging_cleanup(logging_context_t *ctx) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    if (ctx->initialized) {
        LOG_INFO(ctx, "Shutting down logging system");
    }
    
    if (ctx->log_file) {
        fclose(ctx->log_file);
    }
    
    if (ctx->serial_fd >= 0) {
        close(ctx->serial_fd);
    }
    
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
    
    return MICARRAY_SUCCESS;
}

void log_message(logging_context_t *ctx, log_level_t level, const char *format, ...) {
    if (!ctx || !ctx->initialized || level < ctx->config.log_level) {
        return;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    va_list args;
    va_start(args, format);
    
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    
    char log_line[1200];
    snprintf(log_line, sizeof(log_line), "[%s] %s: %s\n", 
             timestamp, log_level_strings[level], message);
    
    printf("%s", log_line);
    
    if (ctx->log_file) {
        fprintf(ctx->log_file, "%s", log_line);
        fflush(ctx->log_file);
    }
    
    if (ctx->serial_fd >= 0) {
        write(ctx->serial_fd, log_line, strlen(log_line));
    }
    
    va_end(args);
    
    pthread_mutex_unlock(&ctx->mutex);
}

void log_location_data(logging_context_t *ctx, const sound_location_t *location) {
    if (!ctx || !location) {
        return;
    }
    
    LOG_INFO(ctx, "LOCATION: x=%.3f, y=%.3f, z=%.3f, confidence=%.3f", 
             location->x, location->y, location->z, location->confidence);
}

void log_noise_metrics(logging_context_t *ctx, float noise_before, float noise_after) {
    if (!ctx) {
        return;
    }
    
    float reduction_db = 20.0f * log10f(noise_before / (noise_after + 1e-10f));
    
    LOG_INFO(ctx, "NOISE_REDUCTION: before=%.3f, after=%.3f, reduction=%.1fdB", 
             noise_before, noise_after, reduction_db);
}

void log_audio_levels(logging_context_t *ctx, float *levels, int num_channels) {
    if (!ctx || !levels) {
        return;
    }
    
    char level_str[256] = "";
    char temp[32];
    
    for (int i = 0; i < num_channels; i++) {
        snprintf(temp, sizeof(temp), "ch%d=%.3f", i, levels[i]);
        if (i > 0) {
            strcat(level_str, ", ");
        }
        strcat(level_str, temp);
    }
    
    LOG_INFO(ctx, "AUDIO_LEVELS: %s", level_str);
}

int logging_set_level(logging_context_t *ctx, log_level_t level) {
    if (!ctx) {
        return MICARRAY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    ctx->config.log_level = level;
    pthread_mutex_unlock(&ctx->mutex);
    
    LOG_INFO(ctx, "Log level changed to %s", log_level_strings[level]);
    
    return MICARRAY_SUCCESS;
}
