### **libmicarray: Multi-Microphone Array Processing Library for Raspberry Pi**

**libmicarray** is a C/C++ library designed for Raspberry Pi (version 5 and above) to handle multiple microphone arrays, such as the **7+1 Inmp441** array, connected through **I2S** for **stereo localization** and **noise reduction**. The library processes the captured audio to output noise-reduced and localized sound to the device’s audio output. Additionally, **DMA (Direct Memory Access)** is used to transfer data, reducing CPU load, and all configurations and logs are managed through a dedicated configuration file and serial port logs.

---

### **Key Features**

1. **Multiple Microphone Array Support**:

   * Supports **7+1 Inmp441 microphone arrays** (or other compatible I2S-based microphone arrays) connected to the Raspberry Pi via **I2S** interface.
   * Configurable support for multiple microphones for **sound localization** and **spatial processing**.

2. **Noise Reduction**:

   * Implements **real-time noise reduction algorithms** to filter out background noise, improving the clarity of the captured sound.

3. **Localization Algorithm**:

   * Uses the multiple microphone array data to calculate the **location** of the sound source.
   * The localization algorithm computes stereo sound positioning based on input from the microphone array.

4. **DMA (Direct Memory Access)**:

   * **DMA** is used for **data transfer** between the microphones and the Raspberry Pi, significantly reducing **CPU load** compared to standard data transfer methods.
   * DMA optimizes performance and allows the Raspberry Pi to handle other tasks concurrently without heavy processing demands.

5. **Configuration File**:

   * A dedicated configuration file to set up microphone arrays, input/output options, and algorithm parameters.
   * Allows users to define microphone arrays, specify I2S data pins, configure DMA buffers, and set up processing algorithms.

6. **Audio Output**:

   * The processed, noise-reduced, and localized audio is sent to the **audio output** of the Raspberry Pi, either through analog or digital audio out, depending on the setup.

7. **Serial Port Logs**:

   * Logs the **localization data** and the microphone input information to the **serial port**, allowing external systems or debugging tools to access this information.
   * Logs can be used for diagnostics or monitoring real-time sound localization and noise reduction performance.

8. **Stereo Localization**:

   * The captured audio is processed to produce **stereo localization**, simulating a **directional sound field** using the microphone array data.
   * The audio is output in **stereo format**, simulating the source location.

---

### **System Architecture**

1. **Microphone Array Interface**:

   * The microphone arrays (e.g., 7+1 Inmp441) are connected to the **Raspberry Pi’s I2S interface**, allowing multiple microphones to feed audio data into the system.
   * **I2S protocol** is used for high-fidelity, low-latency audio capture.

2. **Noise Reduction and Localization**:

   * **Noise reduction** algorithms work in real-time to clean the audio signal before any further processing.
   * The **localization algorithm** analyzes the time differences between the signals received from different microphones to estimate the sound source’s location and direction.
   * **Stereo localization** maps the location of the sound source to appropriate stereo output channels.

3. **DMA Integration**:

   * **DMA channels** handle the audio data transfer from the microphone array to memory. This reduces the load on the CPU and enables efficient handling of real-time audio data.
   * DMA buffers are configured to transfer data in blocks, ensuring minimal latency and maximum throughput.

4. **Configuration File**:

   * The configuration file (`micarray.conf`) allows users to set up the system by defining microphone arrays, DMA settings, input/output settings, and localization algorithm parameters.

   Example configuration:

   ```plaintext
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
   output_device = "headphones"
   volume = 0.8
   ```

5. **Output**:

   * The **processed audio** is output to the configured audio device (e.g., analog, HDMI, or Bluetooth audio output).
   * Users can choose stereo or multichannel output based on the microphone array configuration and the localization algorithm.

6. **Logging and Monitoring**:

   * Serial logs are generated for debugging and monitoring purposes. Logs include:

     * **Sound source location** (x, y, z coordinates)
     * **Noise reduction metrics** (before/after reduction)
     * **Audio signal levels**
   * These logs can be used for tuning the system or analyzing the performance over time.

---

### **Detailed Flow**

1. **Initialization**:

   * The system initializes the microphone arrays based on the configuration file.
   * DMA channels are configured for efficient data transfer.
   * Noise reduction algorithms are set up and ready to process the incoming data.
   * Audio output configuration is prepared.

2. **Data Capture**:

   * The microphone arrays continuously capture audio in real time.
   * Data is transferred via DMA to the memory buffer for processing.

3. **Noise Reduction**:

   * The captured audio undergoes noise reduction processing.
   * The **spectral subtraction** algorithm (or other chosen algorithms) is applied to clean the sound from background noise.

4. **Localization**:

   * Using the time-of-arrival differences between the microphones, the **localization algorithm** calculates the direction and distance of the sound source.
   * The output is a **stereo positioning** of the sound, simulating the sound's location in space.

5. **Audio Output**:

   * The processed, noise-reduced, and localized audio is sent to the **audio output**.
   * Depending on the configuration, the audio can be output in **stereo** or **multichannel** format.

6. **Logging**:

   * Logs are written to the **serial port** containing useful debugging information and system performance data.

---

### **Example Configuration File**

```plaintext
# micarray.conf

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
output_device = "headphones"
volume = 0.8

[Logging]
enable_serial_logging = true
log_file = "/var/log/micarray.log"
```

---

### **Usage Example**

1. **Start the system**:

   * With a configuration file, simply run the library to begin capturing and processing audio.

   ```bash
   ./libmicarray --config /path/to/micarray.conf
   ```

2. **Access logs via serial port**:

   * The serial port will provide real-time logs for localization and noise reduction.

   ```bash
   tail -f /dev/ttyUSB0
   ```

3. **Output processed audio**:

   * The audio can be monitored via the selected output device (e.g., headphones, speakers).

   ```bash
   aplay /dev/sound/output
   ```

---

### **Conclusion**

**libmicarray** provides a **robust solution** for handling multi-microphone arrays, **noise reduction**, and **sound localization**. It efficiently utilizes **DMA** for low CPU load and is highly configurable through a simple **configuration file**. The system is designed to process captured sound data in real-time, outputting **stereo-localized audio** for a seamless listening experience. This library is ideal for applications such as **sound source localization**, **voice recognition**, and **real-time audio processing**.

