# libmicarray

**libmicarray** is a C/C++ library designed for Raspberry Pi (version 5 and above) to handle multiple microphone arrays, such as the **7+1 Inmp441** array, connected through **I2S** for **stereo localization** and **noise reduction**.

## Features

- **Multiple Microphone Array Support**: Supports 7+1 Inmp441 microphone arrays via I2S interface
- **Real-time Noise Reduction**: Implements spectral subtraction algorithms
- **Sound Localization**: Uses time-of-arrival differences for 3D sound positioning
- **DMA Integration**: Efficient data transfer with reduced CPU load
- **Stereo Audio Output**: Processes localized audio to stereo output
- **Serial Port Logging**: Real-time logging of localization data and metrics
- **Configurable**: Comprehensive configuration file support

## Requirements

### Hardware
- Raspberry Pi 5 or newer
- 7+1 Inmp441 microphone array (or compatible I2S microphones)
- Audio output device (headphones, speakers, etc.)

### Software Dependencies
- GCC compiler
- ALSA development libraries (`libasound2-dev`)
- FFTW3 development libraries (`libfftw3-dev`)
- pthread library
- Standard C math library

### Installation of Dependencies (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential libasound2-dev libfftw3-dev
```

## Building

1. Clone or extract the source code
2. Navigate to the project directory
3. Build the library and executable:

```bash
make all
```

### Build Targets

- `make all` - Build library and executable (default)
- `make clean` - Remove build files
- `make debug` - Build with debug symbols
- `make install` - Install system-wide (requires sudo)
- `make uninstall` - Remove installed files
- `make config` - Create example configuration file
- `make package` - Create distribution package

## Configuration

Create a configuration file (default: `micarray.conf`):

```bash
make config
```

Example configuration:

```ini
[General]
log_level = "INFO"

[MicrophoneArray]
num_microphones = 8
mic_spacing = 15mm
i2s_bus = 1
dma_buffer_size = 1024
sample_rate = 16000

[NoiseReduction]
enable = true
noise_threshold = 0.05
algorithm = "spectral_subtraction"

[AudioOutput]
output_device = "default"
volume = 0.8

[Logging]
enable_serial_logging = true
log_file = "/var/log/micarray.log"
```

## Usage

### Command Line Interface

```bash
# Basic usage with default config
./bin/libmicarray

# Specify configuration file
./bin/libmicarray --config /path/to/micarray.conf

# Set volume and run as daemon
./bin/libmicarray --volume 0.8 --daemon

# Show help
./bin/libmicarray --help

# Show version
./bin/libmicarray --version
```

### Library API

```c
#include "libmicarray.h"

int main() {
    micarray_context_t *ctx;
    
    // Initialize with configuration file
    int result = micarray_init(&ctx, "micarray.conf");
    if (result != MICARRAY_SUCCESS) {
        fprintf(stderr, "Init failed: %s\n", micarray_get_error_string(result));
        return -1;
    }
    
    // Start processing
    micarray_start(ctx);
    
    // Get current sound location
    sound_location_t location;
    micarray_get_location(ctx, &location);
    printf("Sound at: x=%.2f, y=%.2f, z=%.2f\n", 
           location.x, location.y, location.z);
    
    // Cleanup
    micarray_stop(ctx);
    micarray_cleanup(ctx);
    
    return 0;
}
```

## Hardware Setup

### I2S Microphone Array Connection

Connect your 7+1 Inmp441 microphone array to the Raspberry Pi I2S pins:

- **BCK (Bit Clock)**: GPIO 18
- **WS (Word Select)**: GPIO 19  
- **DATA**: GPIO 20
- **VCC**: 3.3V
- **GND**: Ground

### Audio Output

Configure your preferred audio output device in the configuration file:
- `"default"` - System default audio device
- `"hw:0,0"` - Hardware device 0, subdevice 0
- `"plughw:1,0"` - USB audio device

## System Integration

### Systemd Service

Create a systemd service for automatic startup:

```bash
sudo tee /etc/systemd/system/libmicarray.service << EOF
[Unit]
Description=libmicarray Multi-Microphone Array Processing
After=network.target sound.target

[Service]
Type=forking
ExecStart=/usr/local/bin/libmicarray --config /etc/micarray.conf --daemon
Restart=always
User=pi
Group=audio

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl enable libmicarray
sudo systemctl start libmicarray
```

### Permissions

Ensure your user has access to audio and serial devices:

```bash
sudo usermod -a -G audio,dialout $USER
```

## Troubleshooting

### Common Issues

1. **I2S Interface Not Found**
   - Ensure I2S is enabled in `/boot/config.txt`:
   ```
   dtparam=i2s=on
   ```
   - Reboot after changes

2. **Audio Output Issues**
   - Check ALSA configuration: `aplay -l`
   - Test audio output: `speaker-test -t wav`

3. **Permission Denied**
   - Add user to audio group: `sudo usermod -a -G audio $USER`
   - Check device permissions: `ls -l /dev/snd/`

4. **DMA Access Issues**
   - Run with sudo for hardware access
   - Check kernel modules: `lsmod | grep snd`

### Logging

Monitor logs for debugging:

```bash
# Real-time log monitoring
tail -f /var/log/micarray.log

# Serial port logs (if enabled)
tail -f /dev/ttyUSB0
```

## Performance Optimization

### CPU Usage
- Adjust `dma_buffer_size` for optimal performance
- Use hardware-specific compiler flags: `-mcpu=cortex-a76`
- Enable CPU governor performance mode

### Latency
- Reduce buffer sizes for lower latency
- Use real-time scheduling priority
- Optimize I2S sample rates

## API Reference

### Core Functions

- `micarray_init()` - Initialize library context
- `micarray_start()` - Start audio processing
- `micarray_stop()` - Stop audio processing  
- `micarray_cleanup()` - Clean up resources
- `micarray_get_location()` - Get current sound location
- `micarray_set_volume()` - Set output volume

### Error Codes

- `MICARRAY_SUCCESS` - Operation successful
- `MICARRAY_ERROR_INIT` - Initialization error
- `MICARRAY_ERROR_CONFIG` - Configuration error
- `MICARRAY_ERROR_I2S` - I2S interface error
- `MICARRAY_ERROR_DMA` - DMA error
- `MICARRAY_ERROR_AUDIO_OUTPUT` - Audio output error
- `MICARRAY_ERROR_MEMORY` - Memory allocation error
- `MICARRAY_ERROR_INVALID_PARAM` - Invalid parameter

## License

This project is provided as-is for educational and research purposes.

## Contributing

Contributions are welcome! Please ensure:
- Code follows the existing style
- All functions are documented
- Memory management is handled properly
- Thread safety is maintained

## Support

For issues and questions:
1. Check the troubleshooting section
2. Review log files for error messages
3. Verify hardware connections
4. Test with minimal configuration
