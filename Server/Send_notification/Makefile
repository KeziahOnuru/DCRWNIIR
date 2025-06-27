# Makefile for FCM Door Close Reminder

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE
LIBS = -lcurl -ljson-c -lssl -lcrypto

# File names
TARGET = door_reminder
SOURCES = main.c fcm_token.c fcm_notification.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = fcm_token.h fcm_notification.h

# Default rule
all: $(TARGET)

# Compile the main program
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libcurl4-openssl-dev libjson-c-dev libssl-dev build-essential

# Test build
test: $(TARGET)
	@echo "Compilation successful!"
	@echo "To test, make sure the service account JSON file is available."

# Help
help:
	@echo "Available commands:"
	@echo "  make                  - Compiles the program"
	@echo "  make clean            - Removes compiled files"
	@echo "  make install-deps     - Installs dependencies (Ubuntu/Debian)"
	@echo "  make install-deps-rpm - Installs dependencies (CentOS/RHEL/Fedora)"
	@echo "  make test             - Tests compilation"
	@echo "  make help             - Shows this help message"

.PHONY: all clean install-deps install-deps-rpm test help
