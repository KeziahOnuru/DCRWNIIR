# Door Monitoring System - Modular Architecture Makefile
# Supports multi-directory project structure with separate modules

# Project information
PROJECT_NAME = door_monitor
VERSION = 1.0.0

# Directories
BLUETOOTH_DIR = Bluetooth_Host
DRIVER_DIR = Driver 
NOTIFICATION_DIR = Send_notification
BUILD_DIR = build
INSTALL_DIR = /usr/local/bin

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -pthread
DEBUG_FLAGS = -g -DDEBUG_LOGGING
INCLUDES = -I. -I$(BLUETOOTH_DIR) -I$(DRIVER_DIR) -I$(NOTIFICATION_DIR)

# Libraries
LIBS = -lbluetooth -lcurl -ljson-c -lssl -lcrypto -lwiringPi -lpthread

# Modular source files
BLUETOOTH_SOURCES = $(BLUETOOTH_DIR)/main.c \
                   $(BLUETOOTH_DIR)/logger.c \
                   $(BLUETOOTH_DIR)/device_manager.c \
                   $(BLUETOOTH_DIR)/bluetooth_server.c

DRIVER_SOURCES = $(wildcard $(DRIVER_DIR)/*.c)  
NOTIFICATION_SOURCES = $(wildcard $(NOTIFICATION_DIR)/*.c)

# All source files
SOURCES = $(BLUETOOTH_SOURCES) $(DRIVER_SOURCES) $(NOTIFICATION_SOURCES)

# Object files (place in build directory with module prefixes)
BLUETOOTH_OBJECTS = $(BUILD_DIR)/main.o \
                   $(BUILD_DIR)/logger.o \
                   $(BUILD_DIR)/device_manager.o \
                   $(BUILD_DIR)/bluetooth_server.o

DRIVER_OBJECTS = $(patsubst $(DRIVER_DIR)/%.c,$(BUILD_DIR)/driver_%.o,$(DRIVER_SOURCES))
NOTIFICATION_OBJECTS = $(patsubst $(NOTIFICATION_DIR)/%.c,$(BUILD_DIR)/notification_%.o,$(NOTIFICATION_SOURCES))

OBJECTS = $(BLUETOOTH_OBJECTS) $(DRIVER_OBJECTS) $(NOTIFICATION_OBJECTS)

# Header files for dependency tracking
HEADERS = config.h \
          $(wildcard $(BLUETOOTH_DIR)/*.h) \
          $(wildcard $(DRIVER_DIR)/*.h) \
          $(wildcard $(NOTIFICATION_DIR)/*.h)

# Default target
.PHONY: all
all: $(PROJECT_NAME)

# Create build directory
$(BUILD_DIR):
	@echo "Creating build directory..."
	@mkdir -p $(BUILD_DIR)

# Main executable
$(PROJECT_NAME): $(BUILD_DIR) $(OBJECTS)
	@echo "Linking $(PROJECT_NAME)..."
	@$(CC) $(OBJECTS) $(LIBS) -o $(PROJECT_NAME)
	@echo "✅ Build complete: $(PROJECT_NAME)"

# Bluetooth Host module object files
$(BUILD_DIR)/main.o: $(BLUETOOTH_DIR)/main.c $(HEADERS)
	@echo "Compiling main module: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/logger.o: $(BLUETOOTH_DIR)/logger.c $(HEADERS)
	@echo "Compiling logger module: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/device_manager.o: $(BLUETOOTH_DIR)/device_manager.c $(HEADERS)
	@echo "Compiling device manager module: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/bluetooth_server.o: $(BLUETOOTH_DIR)/bluetooth_server.c $(HEADERS)
	@echo "Compiling Bluetooth server module: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Driver object files  
$(BUILD_DIR)/driver_%.o: $(DRIVER_DIR)/%.c $(HEADERS)
	@echo "Compiling Driver module: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Notification object files
$(BUILD_DIR)/notification_%.o: $(NOTIFICATION_DIR)/%.c $(HEADERS)
	@echo "Compiling Notification module: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Debug build
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(PROJECT_NAME)
	@echo "🐛 Debug build complete"

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f $(PROJECT_NAME)
	@echo "🧹 Clean complete"

# Install to system
.PHONY: install
install: $(PROJECT_NAME)
	@echo "Installing $(PROJECT_NAME) to $(INSTALL_DIR)..."
	@sudo cp $(PROJECT_NAME) $(INSTALL_DIR)/
	@sudo chmod +x $(INSTALL_DIR)/$(PROJECT_NAME)
	@echo "📦 Installation complete"

# Uninstall from system
.PHONY: uninstall
uninstall:
	@echo "Uninstalling $(PROJECT_NAME)..."
	@sudo rm -f $(INSTALL_DIR)/$(PROJECT_NAME)
	@echo "🗑️  Uninstallation complete"

# Check dependencies
.PHONY: check-deps
check-deps:
	@echo "Checking system dependencies..."
	@pkg-config --exists libbluetooth || echo "❌ libbluetooth-dev missing"
	@pkg-config --exists libcurl || echo "❌ libcurl4-openssl-dev missing"  
	@pkg-config --exists json-c || echo "❌ libjson-c-dev missing"
	@pkg-config --exists openssl || echo "❌ libssl-dev missing"
	@which gpio >/dev/null 2>&1 || echo "❌ WiringPi missing"
	@echo "✅ Dependency check complete"

# Install dependencies (Debian/Ubuntu)
.PHONY: install-deps
install-deps:
	@echo "Installing system dependencies..."
	@sudo apt-get update
	@sudo apt-get install -y \
		libbluetooth-dev \
		libcurl4-openssl-dev \
		libjson-c-dev \
		libssl-dev \
		build-essential
	@echo "📦 Dependencies installed"

# Run with appropriate privileges
.PHONY: run
run: $(PROJECT_NAME)
	@echo "Starting $(PROJECT_NAME) with root privileges..."
	@sudo ./$(PROJECT_NAME)

# Create systemd service file
.PHONY: service
service: install
	@echo "Creating systemd service..."
	@sudo tee /etc/systemd/system/door-monitor.service > /dev/null << 'EOF'
[Unit]
Description=Door Monitoring System
After=network.target bluetooth.target

[Service]
Type=simple
User=root
ExecStart=$(INSTALL_DIR)/$(PROJECT_NAME)
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
	@sudo systemctl daemon-reload
	@echo "🔧 Service created: door-monitor.service"
	@echo "   Enable: sudo systemctl enable door-monitor"
	@echo "   Start:  sudo systemctl start door-monitor"

# Show project structure
.PHONY: structure
structure:
	@echo "📁 Modular Project Structure:"
	@echo "├── config.h (Centralized configuration)"
	@echo "├── $(BLUETOOTH_DIR)/"
	@echo "│   ├── main.c (System entry point)"
	@echo "│   ├── logger.c/h (Logging system)"
	@echo "│   ├── device_manager.c/h (BLE device management)"
	@echo "│   ├── bluetooth_server.c/h (L2CAP server)"
	@echo "│   └── BLEHost.h (Main system header)"
	@echo "├── $(DRIVER_DIR)/"
	@ls -la $(DRIVER_DIR)/ | sed 's/^/│   /'
	@echo "├── $(NOTIFICATION_DIR)/"
	@ls -la $(NOTIFICATION_DIR)/ | sed 's/^/│   /'
	@echo "└── Makefile (Modular build system)"

# Check modular architecture
.PHONY: check-modules
check-modules:
	@echo "🔍 Checking modular architecture..."
	@echo "Main modules:"
	@test -f $(BLUETOOTH_DIR)/main.c && echo "  ✅ main.c (System entry point)" || echo "  ❌ main.c missing"
	@test -f $(BLUETOOTH_DIR)/logger.c && echo "  ✅ logger.c (Logging system)" || echo "  ❌ logger.c missing"
	@test -f $(BLUETOOTH_DIR)/device_manager.c && echo "  ✅ device_manager.c (Device management)" || echo "  ❌ device_manager.c missing"
	@test -f $(BLUETOOTH_DIR)/bluetooth_server.c && echo "  ✅ bluetooth_server.c (BLE server)" || echo "  ❌ bluetooth_server.c missing"
	@echo "Configuration:"
	@test -f config.h && echo "  ✅ config.h (Centralized config)" || echo "  ❌ config.h missing"
	@echo "Legacy files:"
	@test -f $(BLUETOOTH_DIR)/BLEHost.c && echo "  ⚠️  BLEHost.c (should be removed - now modular)" || echo "  ✅ BLEHost.c removed (good)"

# Module-specific builds (for testing individual modules)
.PHONY: build-logger
build-logger: $(BUILD_DIR) $(BUILD_DIR)/logger.o
	@echo "✅ Logger module compiled"

.PHONY: build-device-manager  
build-device-manager: $(BUILD_DIR) $(BUILD_DIR)/device_manager.o
	@echo "✅ Device manager module compiled"

.PHONY: build-bluetooth-server
build-bluetooth-server: $(BUILD_DIR) $(BUILD_DIR)/bluetooth_server.o
	@echo "✅ Bluetooth server module compiled"

# Display help
.PHONY: help
help:
	@echo "🚀 Door Monitoring System - Modular Build Commands"
	@echo "=================================================="
	@echo ""
	@echo "Basic Commands:"
	@echo "  make          - Build the complete modular project"
	@echo "  make debug    - Build with debug symbols and logging"
	@echo "  make clean    - Remove build artifacts" 
	@echo "  make run      - Build and run with root privileges"
	@echo ""
	@echo "Modular Architecture:"
	@echo "  make structure      - Show modular project structure"
	@echo "  make check-modules  - Verify modular architecture"
	@echo "  make build-logger   - Build logger module only"
	@echo "  make build-device-manager - Build device manager only"
	@echo "  make build-bluetooth-server - Build BLE server only"
	@echo ""
	@echo "System Integration:"
	@echo "  make install-deps - Install system dependencies"
	@echo "  make check-deps   - Check for missing dependencies"
	@echo "  make install      - Install to /usr/local/bin"
	@echo "  make uninstall    - Remove from system"
	@echo "  make service      - Create systemd service"
	@echo ""
	@echo "Information:"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Modular Architecture Benefits:"
	@echo "  • Single Responsibility Principle"
	@echo "  • Independent module testing"
	@echo "  • Better maintainability"
	@echo "  • Code reusability"
	@echo "  • Easier debugging"

# Prevent make from treating file names as targets
.SUFFIXES: