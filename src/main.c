#define _GNU_SOURCE
#include "libmicarray.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>

static volatile bool g_running = true;
static micarray_context_t *g_micarray_ctx = NULL;

static void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    g_running = false;
    
    if (g_micarray_ctx) {
        micarray_stop(g_micarray_ctx);
    }
}

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -c, --config FILE    Configuration file path (default: micarray.conf)\n");
    printf("  -v, --volume LEVEL   Set volume level (0.0-1.0)\n");
    printf("  -d, --daemon         Run as daemon\n");
    printf("  -h, --help           Show this help message\n");
    printf("  --version            Show version information\n");
    printf("\nExamples:\n");
    printf("  %s --config /etc/micarray.conf\n", program_name);
    printf("  %s --volume 0.8 --daemon\n", program_name);
}

static void print_version(void) {
    printf("libmicarray %s\n", micarray_get_version());
    printf("Multi-microphone array processing library for Raspberry Pi\n");
    printf("Copyright (c) 2024\n");
}

static void print_status(micarray_context_t *ctx) {
    sound_location_t location;
    
    if (micarray_get_location(ctx, &location) == MICARRAY_SUCCESS) {
        printf("\rLocation: x=%.2f, y=%.2f, z=%.2f, confidence=%.2f", 
               location.x, location.y, location.z, location.confidence);
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    const char *config_file = "micarray.conf";
    float volume = -1.0f;
    bool daemon_mode = false;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"volume", required_argument, 0, 'v'},
        {"daemon", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 0},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:v:dh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'v':
                volume = strtof(optarg, NULL);
                if (volume < 0.0f || volume > 1.0f) {
                    fprintf(stderr, "Error: Volume must be between 0.0 and 1.0\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'd':
                daemon_mode = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 0:
                if (strcmp(long_options[option_index].name, "version") == 0) {
                    print_version();
                    return EXIT_SUCCESS;
                }
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }
    
    if (access(config_file, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot read configuration file '%s'\n", config_file);
        return EXIT_FAILURE;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("libmicarray %s - Multi-microphone array processing\n", micarray_get_version());
    printf("Configuration file: %s\n", config_file);
    
    int result = micarray_init(&g_micarray_ctx, config_file);
    if (result != MICARRAY_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize libmicarray: %s\n", 
                micarray_get_error_string(result));
        return EXIT_FAILURE;
    }
    
    if (volume >= 0.0f) {
        result = micarray_set_volume(g_micarray_ctx, volume);
        if (result != MICARRAY_SUCCESS) {
            fprintf(stderr, "Warning: Failed to set volume: %s\n", 
                    micarray_get_error_string(result));
        } else {
            printf("Volume set to %.1f\n", volume);
        }
    }
    
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Error: Failed to fork daemon process\n");
            micarray_cleanup(g_micarray_ctx);
            return EXIT_FAILURE;
        }
        
        if (pid > 0) {
            printf("Daemon started with PID %d\n", pid);
            micarray_cleanup(g_micarray_ctx);
            return EXIT_SUCCESS;
        }
        
        setsid();
        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
    
    result = micarray_start(g_micarray_ctx);
    if (result != MICARRAY_SUCCESS) {
        fprintf(stderr, "Error: Failed to start microphone array: %s\n", 
                micarray_get_error_string(result));
        micarray_cleanup(g_micarray_ctx);
        return EXIT_FAILURE;
    }
    
    if (!daemon_mode) {
        printf("Microphone array started successfully. Press Ctrl+C to stop.\n");
        printf("Real-time status (location and confidence):\n");
        
        while (g_running) {
            print_status(g_micarray_ctx);
            sleep(1);
        }
        
        printf("\n");
    } else {
        while (g_running) {
            sleep(1);
        }
    }
    
    printf("Shutting down...\n");
    
    result = micarray_stop(g_micarray_ctx);
    if (result != MICARRAY_SUCCESS) {
        fprintf(stderr, "Warning: Error stopping microphone array: %s\n", 
                micarray_get_error_string(result));
    }
    
    result = micarray_cleanup(g_micarray_ctx);
    if (result != MICARRAY_SUCCESS) {
        fprintf(stderr, "Warning: Error during cleanup: %s\n", 
                micarray_get_error_string(result));
    }
    
    printf("Shutdown complete.\n");
    return EXIT_SUCCESS;
}
