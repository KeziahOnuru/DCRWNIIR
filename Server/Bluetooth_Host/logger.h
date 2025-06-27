/**
 * @file logger.h
 * @brief Centralized logging system for Door Monitoring System
 * 
 * This module provides a thread-safe logging system with timestamp support
 * and different log levels. Designed for debugging, monitoring, and
 * production troubleshooting.
 * 
 * Features:
 * - Multiple log levels (INFO, WARN, ERROR)
 * - Automatic timestamping
 * - Thread-safe operations
 * - Configurable output format
 * - Production-ready performance
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

#include "config.h"

// ============================================================================
// LOG LEVEL DEFINITIONS
// ============================================================================

typedef enum {
    LOG_LEVEL_ERROR = 0,    /// Error messages only
    LOG_LEVEL_WARN  = 1,    /// Warnings and errors
    LOG_LEVEL_INFO  = 2,    /// All messages
    LOG_LEVEL_DEBUG = 3     /// Debug messages (if compiled with DEBUG)
} LogLevel;

// ============================================================================
// LOGGING FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the logging system
 * @return 0 on success, negative on error
 * 
 * Sets up logging infrastructure including mutex initialization
 * and configuration validation. Must be called before using
 * other logging functions.
 */
int logger_init(void);

/**
 * @brief Cleanup logging system resources
 * 
 * Properly destroys mutexes and cleans up logging resources.
 * Called during system shutdown.
 */
void logger_cleanup(void);

/**
 * @brief Set the current log level
 * @param level Minimum log level to display
 * 
 * Controls which messages are displayed based on severity.
 * Messages below this level are filtered out.
 */
void logger_set_level(LogLevel level);

/**
 * @brief Get the current log level
 * @return Current minimum log level
 */
LogLevel logger_get_level(void);

/**
 * @brief Generic logging function with timestamp and level
 * @param level Log level string (INFO, WARN, ERROR, DEBUG)
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * 
 * Thread-safe logging function that adds timestamps and formats
 * messages consistently. Used internally by convenience macros.
 */
void log_message(const char *level, const char *format, ...);

/**
 * @brief Log message with specific level check
 * @param level LogLevel enumeration value
 * @param level_str String representation of level
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * 
 * Internal function that checks log level before formatting message.
 * Provides better performance by avoiding unnecessary string formatting.
 */
void log_message_level(LogLevel level, const char *level_str, const char *format, ...);

// ============================================================================
// CONVENIENCE MACROS
// ============================================================================

/// Log an informational message
#define LOG_INFO(fmt, ...) log_message_level(LOG_LEVEL_INFO, "INFO", fmt, ##__VA_ARGS__)

/// Log a warning message
#define LOG_WARN(fmt, ...) log_message_level(LOG_LEVEL_WARN, "WARN", fmt, ##__VA_ARGS__)

/// Log an error message
#define LOG_ERROR(fmt, ...) log_message_level(LOG_LEVEL_ERROR, "ERROR", fmt, ##__VA_ARGS__)

/// Log a debug message (only if DEBUG is defined)
#ifdef DEBUG_LOGGING
#define LOG_DEBUG(fmt, ...) log_message_level(LOG_LEVEL_DEBUG, "DEBUG", fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) do { } while(0)
#endif

// ============================================================================
// SPECIALIZED LOGGING FUNCTIONS
// ============================================================================

/**
 * @brief Log system startup information
 * @param system_name Name of the system/component starting
 * @param version Version string
 * 
 * Standardized startup logging with system information.
 */
void log_system_startup(const char *system_name, const char *version);

/**
 * @brief Log system shutdown information
 * @param system_name Name of the system/component shutting down
 * 
 * Standardized shutdown logging for clean termination tracking.
 */
void log_system_shutdown(const char *system_name);

/**
 * @brief Log device connection event
 * @param mac_address MAC address of connecting device
 * @param device_count Current total device count
 * 
 * Specialized logging for device management events.
 */
void log_device_connect(const char *mac_address, int device_count);

/**
 * @brief Log device disconnection event
 * @param mac_address MAC address of disconnecting device
 * @param device_count Remaining device count
 * @param reason Disconnection reason (timeout, explicit, error)
 * 
 * Specialized logging for device management events.
 */
void log_device_disconnect(const char *mac_address, int device_count, const char *reason);

/**
 * @brief Log notification sending event
 * @param success Whether notification was sent successfully
 * @param error_code Error code if failed (0 if successful)
 * @param recipient_preview Preview of recipient token
 * 
 * Specialized logging for FCM notification events.
 */
void log_notification_event(int success, int error_code, const char *recipient_preview);

/**
 * @brief Log door state change event
 * @param old_state Previous door state
 * @param new_state New door state
 * 
 * Specialized logging for door sensor events.
 */
void log_door_state_change(int old_state, int new_state);

#endif // LOGGER_H