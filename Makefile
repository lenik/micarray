CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -fPIC
LDFLAGS = -shared
LIBS = -lm -lpthread -lasound -lfftw3f

# Directories
SRCDIR = src
OBJDIR = obj
LIBDIR = lib
BINDIR = bin

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
LIB_OBJECTS = $(filter-out $(OBJDIR)/main.o, $(OBJECTS))

# Targets
LIBRARY = $(LIBDIR)/libmicarray.so
EXECUTABLE = $(BINDIR)/libmicarray
STATIC_LIB = $(LIBDIR)/libmicarray.a

# Default target
all: directories $(LIBRARY) $(EXECUTABLE) $(STATIC_LIB)

# Create directories
directories:
	@mkdir -p $(OBJDIR) $(LIBDIR) $(BINDIR) $(TEST_OBJDIR) $(TEST_BINDIR)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build shared library
$(LIBRARY): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Build static library
$(STATIC_LIB): $(LIB_OBJECTS)
	ar rcs $@ $^

# Build executable
$(EXECUTABLE): $(OBJDIR)/main.o $(LIBRARY)
	$(CC) -o $@ $< -L$(LIBDIR) -lmicarray $(LIBS)

# Install targets
install: all
	sudo cp $(LIBRARY) /usr/local/lib/
	sudo cp $(STATIC_LIB) /usr/local/lib/
	sudo cp $(SRCDIR)/libmicarray.h /usr/local/include/
	sudo cp $(EXECUTABLE) /usr/local/bin/
	sudo ldconfig

# Uninstall
uninstall:
	sudo rm -f /usr/local/lib/libmicarray.so
	sudo rm -f /usr/local/lib/libmicarray.a
	sudo rm -f /usr/local/include/libmicarray.h
	sudo rm -f /usr/local/bin/libmicarray

# Clean build files
clean:
	rm -rf $(OBJDIR) $(LIBDIR) $(BINDIR) $(TEST_OBJDIR) $(TEST_BINDIR)

# Development targets
debug: CFLAGS += -g -DDEBUG
debug: all

# Create example configuration
config:
	@echo "Creating example configuration file..."
	@cat > micarray.conf << 'EOF'
# libmicarray configuration file

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
EOF
	@echo "Configuration file 'micarray.conf' created."

# Test targets
TEST_SRCDIR = tests
TEST_OBJDIR = test_obj
TEST_BINDIR = test_bin

TEST_SOURCES = $(wildcard $(TEST_SRCDIR)/*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_SRCDIR)/%.c=$(TEST_OBJDIR)/%.o)
TEST_EXECUTABLES = $(filter-out $(TEST_BINDIR)/run_tests, $(TEST_OBJECTS:$(TEST_OBJDIR)/%.o=$(TEST_BINDIR)/%))
TEST_RUNNER = $(TEST_BINDIR)/run_tests

# Build and run tests
test: test-build
	@echo "Running test suite..."
	@cd $(TEST_BINDIR) && ./run_tests

# Build all test executables
test-build: directories $(LIBRARY) $(TEST_EXECUTABLES) $(TEST_RUNNER)

# Create test directories
test-directories:
	@mkdir -p $(TEST_OBJDIR) $(TEST_BINDIR)

# Compile test source files
$(TEST_OBJDIR)/%.o: $(TEST_SRCDIR)/%.c
	@mkdir -p $(TEST_OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

# Build individual test executables
$(TEST_BINDIR)/%: $(TEST_OBJDIR)/%.o $(LIBRARY)
	@mkdir -p $(TEST_BINDIR)
	$(CC) -o $@ $< -L$(LIBDIR) -lmicarray $(LIBS)

# Build test runner
$(TEST_RUNNER): $(TEST_OBJDIR)/run_tests.o
	@mkdir -p $(TEST_BINDIR)
	$(CC) -o $@ $<

# Run specific test
test-%: $(TEST_BINDIR)/test_%
	@cd $(TEST_BINDIR) && ./test_$*

# Clean test files
test-clean:
	rm -rf $(TEST_OBJDIR) $(TEST_BINDIR)

# Package creation
package: all
	@mkdir -p package/libmicarray-$(shell date +%Y%m%d)
	@cp -r $(SRCDIR) package/libmicarray-$(shell date +%Y%m%d)/
	@cp -r $(LIBDIR) package/libmicarray-$(shell date +%Y%m%d)/
	@cp -r $(BINDIR) package/libmicarray-$(shell date +%Y%m%d)/
	@cp Makefile README.md package/libmicarray-$(shell date +%Y%m%d)/
	@cd package && tar -czf libmicarray-$(shell date +%Y%m%d).tar.gz libmicarray-$(shell date +%Y%m%d)
	@echo "Package created: package/libmicarray-$(shell date +%Y%m%d).tar.gz"

# Help target
help:
	@echo "Available targets:"
	@echo "  all       - Build library and executable (default)"
	@echo "  clean     - Remove build files"
	@echo "  install   - Install library and executable system-wide"
	@echo "  uninstall - Remove installed files"
	@echo "  debug     - Build with debug symbols"
	@echo "  config    - Create example configuration file"
	@echo "  test      - Build and run all tests
  test-build - Build test executables only
  test-clean - Clean test build files
  test-<name> - Run specific test (e.g., test-config)"
	@echo "  package   - Create distribution package"
	@echo "  help      - Show this help message"

.PHONY: all directories clean install uninstall debug config test test-build test-clean test-directories package help
