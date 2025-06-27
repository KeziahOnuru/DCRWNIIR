/**
 * @file config.h
 * @brief Centralized configuration file for Door Monitoring System
 * 
 * This file contains all system-wide configuration constants including
 * Firebase settings, BLE parameters, GPIO configuration, and file paths.
 * 
 * Update this file to modify system behavior without changing source code.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// FIREBASE CLOUD MESSAGING CONFIGURATION
// ============================================================================

/// Path to Firebase service account JSON file for FCM authentication
#define FIREBASE_SERVICE_ACCOUNT_PATH "Send_notification/firebase-service-account.json"

/// Firebase project ID for FCM messaging
#define FIREBASE_PROJECT_ID "door-close-reminder"

/// FCM notification title for door reminders
#define FCM_NOTIFICATION_TITLE "Door-close reminder"

/// FCM notification body text
#define FCM_NOTIFICATION_BODY "Room 809 : Don't forget to close the door !"

/// FCM notification data type identifier
#define FCM_NOTIFICATION_DATA_TYPE "door-close-reminder"

// ============================================================================
// BLUETOOTH LOW ENERGY CONFIGURATION
// ============================================================================

/// L2CAP Protocol Service Multiplexer for BLE communication
#define BLE_PSM 0x1001

/// Maximum number of concurrent BLE devices
#define MAX_DEVICES 10

/// FCM token buffer size
#define TOKEN_SIZE 256

/// Communication buffer size for BLE data
#define BUFFER_SIZE 1024

/// Device heartbeat timeout in seconds
#define HEARTBEAT_TIMEOUT 60

/// Minimum FCM token length for validation
#define MIN_FCM_TOKEN_LENGTH 140

// ============================================================================
// GPIO DOOR SENSOR CONFIGURATION
// ============================================================================

/// GPIO pin number (WiringPi numbering) for door sensor
#define DOOR_SENSOR_PIN 25

/// Hardware debounce time in microseconds (3ms)
#define BOUNCE_TIME_US 3000

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================

/// Heartbeat monitoring thread sleep interval in seconds
#define HEARTBEAT_CHECK_INTERVAL 10

/// Select timeout for network operations in seconds
#define NETWORK_SELECT_TIMEOUT 5

/// Maximum HTTP response size for FCM operations
#define MAX_HTTP_RESPONSE_SIZE 8192

/// JWT token maximum size for OAuth operations
#define MAX_JWT_SIZE 4096

/// OAuth token maximum size
#define MAX_OAUTH_TOKEN_SIZE 2048

// ============================================================================
// PROJECT DIRECTORY STRUCTURE
// ============================================================================

/// Bluetooth Host module directory
#define BLUETOOTH_HOST_DIR "Bluetooth_Host"

/// Driver module directory  
#define DRIVER_DIR "Driver"

/// Notification module directory
#define NOTIFICATION_DIR "Send_notification"

// ============================================================================
// LOGGING CONFIGURATION
// ============================================================================

/// Enable debug logging (comment out for production)
// #define DEBUG_LOGGING

/// Timestamp format for log messages
#define LOG_TIMESTAMP_FORMAT "%H:%M:%S"

/// Log buffer size
#define LOG_BUFFER_SIZE 256

// ============================================================================
// NETWORK CONFIGURATION
// ============================================================================

/// Google OAuth2 token endpoint
#define OAUTH_TOKEN_URL "https://oauth2.googleapis.com/token"

/// FCM API base URL template (requires project_id)
#define FCM_API_URL_TEMPLATE "https://fcm.googleapis.com/v1/projects/%s/messages:send"

/// OAuth scope for FCM access
#define OAUTH_SCOPE "https://www.googleapis.com/auth/firebase.messaging"

/// JWT algorithm for OAuth signing
#define JWT_ALGORITHM "RS256"

/// JWT token type
#define JWT_TOKEN_TYPE "JWT"

/// JWT expiration time in seconds (1 hour)
#define JWT_EXPIRATION_TIME 3600

// ============================================================================
// SYSTEM REQUIREMENTS
// ============================================================================

/// Minimum required root privileges check
#define REQUIRE_ROOT_PRIVILEGES 1

/// Enable Firebase service account file validation
#define VALIDATE_FIREBASE_CONFIG 1

/// Enable WiringPi GPIO validation
#define VALIDATE_GPIO_CONFIG 1

/// Enable Bluetooth capability validation  
#define VALIDATE_BLUETOOTH_CONFIG 1

// ============================================================================
// ERROR CODES
// ============================================================================

/// Success return code
#define SUCCESS 0

/// Generic error return code
#define ERROR_GENERIC -1

/// Insufficient privileges error
#define ERROR_PRIVILEGES -2

/// Configuration file not found error
#define ERROR_CONFIG_FILE -3

/// Hardware initialization error
#define ERROR_HARDWARE_INIT -4

/// Network operation error
#define ERROR_NETWORK -5

/// Memory allocation error
#define ERROR_MEMORY -6

/// Invalid parameter error
#define ERROR_INVALID_PARAM -7

#endif // CONFIG_H