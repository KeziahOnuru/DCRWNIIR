/**
 * @file logger.c
 * @brief Implementation of centralized logging system
 * 
 * Thread-safe logging implementation with configurable levels,
 * timestamp formatting, and specialized logging functions for
 * different system events.
 */

#include "logger.h"

// ============================================================================
// STATIC VARIABLES
// ============================================================================

static LogLevel current_log_level = LOG_LEVEL_INFO;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int logger_initialized = 0;

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Get current timestamp string
 * @param buffer Buffer to store timestamp
 * @param buffer_size Size of the buffer
 * @return Pointer to buffer on success, NULL on error
 */
static char* get_timestamp(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    if (strftime(buffer, buffer_size, LOG_TIMESTAMP_FORMAT, tm_info) == 0) {
        return NULL;
    }
    
    return buffer;
}

/**
 * @brief Check if a log level should be displayed
 * @param level Log level to check
 * @return 1 if should be displayed, 0 if filtered out
 */
static int should_log(LogLevel level) {
    return (level <= current_log_level);
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

int logger_init(void) {
    if (logger_initialized) {
        return 0; // Already initialized
    }
    
    // Initialize mutex (already done statically, but reset for safety)
    if (pthread_mutex_init(&log_mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize log mutex\n");
        return -1;
    }
    
    logger_initialized = 1;
    
    // Log initialization
    LOG_INFO("Logging system initialized");
    return 0;
}

void logger_cleanup(void) {
    if (!logger_initialized) {
        return;
    }
    
    LOG_INFO("Shutting down logging system");
    
    pthread_mutex_destroy(&log_mutex);
    logger_initialized = 0;
}

void logger_set_level(LogLevel level) {
    pthread_mutex_lock(&log_mutex);
    current_log_level = level;
    pthread_mutex_unlock(&log_mutex);
    
    const char *level_names[] = {"ERROR", "WARN", "INFO", "DEBUG"};
    LOG_INFO("Log level set to: %s", level_names[level]);
}

LogLevel logger_get_level(void) {
    pthread_mutex_lock(&log_mutex);
    LogLevel level = current_log_level;
    pthread_mutex_unlock(&log_mutex);
    return level;
}

void log_message(const char *level, const char *format, ...) {
    if (!logger_initialized) {
        logger_init(); // Auto-initialize if needed
    }
    
    pthread_mutex_lock(&log_mutex);
    
    // Get timestamp
    char timestamp[32];
    if (get_timestamp(timestamp, sizeof(timestamp)) == NULL) {
        strcpy(timestamp, "??:??:??");
    }
    
    // Print timestamp and level
    printf("[%s %s] ", timestamp, level);
    
    // Print formatted message
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
    fflush(stdout);
    
    pthread_mutex_unlock(&log_mutex);
}

void log_message_level(LogLevel level, const char *level_str, const char *format, ...) {
    if (!should_log(level)) {
        return; // Filter out this log level
    }
    
    if (!logger_initialized) {
        logger_init(); // Auto-initialize if needed
    }
    
    pthread_mutex_lock(&log_mutex);
    
    // Get timestamp
    char timestamp[32];
    if (get_timestamp(timestamp, sizeof(timestamp)) == NULL) {
        strcpy(timestamp, "??:??:??");
    }
    
    // Print timestamp and level
    printf("[%s %s] ", timestamp, level_str);
    
    // Print formatted message
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
    fflush(stdout);
    
    pthread_mutex_unlock(&log_mutex);
}

// ============================================================================
// SPECIALIZED LOGGING FUNCTIONS
// ============================================================================

void log_system_startup(const char *system_name, const char *version) {
    LOG_INFO("=== %s v%s Starting ===", system_name, version);
    LOG_INFO("Process ID: %d", getpid());
    LOG_INFO("User ID: %d", getuid());
    
#ifdef DEBUG_LOGGING
    LOG_DEBUG("Debug logging enabled");
#endif
}

void log_system_shutdown(const char *system_name) {
    LOG_INFO("=== %s Shutting Down ===", system_name);
}

void log_device_connect(const char *mac_address, int device_count) {
    LOG_INFO("Device connected: %s (Total devices: %d)", mac_address, device_count);
}

void log_device_disconnect(const char *mac_address, int device_count, const char *reason) {
    LOG_INFO("Device disconnected: %s - %s (Remaining: %d)", mac_address, reason, device_count);
}

void log_notification_event(int success, int error_code, const char *recipient_preview) {
    if (success) {
        LOG_INFO("Notification sent successfully to: %.15s...", recipient_preview);
    } else {
        LOG_ERROR("Notification failed (code: %d) for: %.15s...", error_code, recipient_preview);
    }
}

void log_door_state_change(int old_state, int new_state) {
    const char *state_names[] = {"LOCKED", "UNLOCKED", "ERROR"};
    const char *old_name = (old_state >= 0 && old_state <= 1) ? state_names[old_state] : "UNKNOWN";
    const char *new_name = (new_state >= 0 && new_state <= 1) ? state_names[new_state] : "ERROR";
    
    LOG_INFO("Door state change: %s -> %s", old_name, new_name);
}