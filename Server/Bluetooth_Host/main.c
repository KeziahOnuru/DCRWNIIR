/**
 * @file main.c
 * @brief Door Monitoring System - Main Entry Point
 * 
 * This file contains the main entry point and system initialization
 * for the Door Monitoring System. It coordinates all subsystems including
 * the door sensor driver, BLE device management, Bluetooth server,
 * and Firebase notification system.
 * 
 * System Architecture:
 * - Door Sensor Driver: GPIO-based door lock/unlock detection
 * - Device Manager: BLE device tracking and heartbeat monitoring
 * - Bluetooth Server: L2CAP server for BLE communication
 * - FCM Notifications: Firebase Cloud Messaging integration
 * - Centralized Logging: Thread-safe logging system
 * 
 * The system intelligently sends door-close reminders only when:
 * 1. All BLE devices have disconnected (no one present)
 * 2. The door sensor indicates UNLOCKED state
 * 3. Valid FCM token is available from last disconnected device
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <pthread.h>

// System configuration and modules
#include "config.h"
#include "logger.h"
#include "DoorStateDriver.h"
#include "device_manager.h"
#include "bluetooth_server.h"
#include "fcm_notification.h"

// ============================================================================
// GLOBAL SYSTEM VARIABLES
// ============================================================================

static DeviceManager g_device_manager = {0};
static BluetoothServer g_bluetooth_server = {0};
static volatile int g_system_running = 1;

// ============================================================================
// SYSTEM INFORMATION
// ============================================================================

#define SYSTEM_NAME "Door Monitoring System"
#define SYSTEM_VERSION "1.0.0"

// ============================================================================
// SIGNAL HANDLING
// ============================================================================

/**
 * @brief Signal handler for graceful shutdown
 * @param sig Signal number received
 * 
 * Handles SIGINT and SIGTERM signals to initiate clean system shutdown.
 * Sets the running flag to false to trigger cleanup procedures.
 */
void signal_handler(int sig) {
    const char *signal_name = (sig == SIGINT) ? "SIGINT" : 
                             (sig == SIGTERM) ? "SIGTERM" : "UNKNOWN";
    
    LOG_INFO("Received signal %s (%d) - initiating shutdown", signal_name, sig);
    g_system_running = 0;
    
    // Stop subsystems
    bluetooth_server_stop(&g_bluetooth_server);
    device_manager_stop_heartbeat(&g_device_manager);
}

/**
 * @brief Install signal handlers for graceful shutdown
 * @return 0 on success, negative on error
 */
static int install_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        LOG_ERROR("Failed to install SIGINT handler");
        return -1;
    }
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_ERROR("Failed to install SIGTERM handler");
        return -1;
    }
    
    // Ignore SIGPIPE (broken pipe) to prevent crashes on socket errors
    signal(SIGPIPE, SIG_IGN);
    
    LOG_INFO("Signal handlers installed");
    return 0;
}

// ============================================================================
// SYSTEM VALIDATION
// ============================================================================

/**
 * @brief Check system privileges and requirements
 * @return 0 if requirements met, negative if not
 */
static int check_system_requirements(void) {
    LOG_INFO("Checking system requirements...");
    
    // Check root privileges
    if (geteuid() != 0) {
        LOG_ERROR("Root privileges required for GPIO and Bluetooth access");
        LOG_ERROR("Please run with: sudo %s", getprogname());
        return ERROR_PRIVILEGES;
    }
    LOG_INFO("Root privileges: OK");
    
    // Check Firebase service account file
    if (access(FIREBASE_SERVICE_ACCOUNT_PATH, R_OK) != 0) {
        LOG_WARN("Firebase service account file not accessible: %s", FIREBASE_SERVICE_ACCOUNT_PATH);
        LOG_WARN("Notifications will not work without valid Firebase credentials");
    } else {
        LOG_INFO("Firebase service account file: OK");
    }
    
    // Check Bluetooth system availability
    if (bluetooth_server_check_system() != 0) {
        LOG_ERROR("Bluetooth system not available");
        return ERROR_HARDWARE_INIT;
    }
    LOG_INFO("Bluetooth system: OK");
    
    LOG_INFO("System requirements check completed");
    return 0;
}

// ============================================================================
// SUBSYSTEM INITIALIZATION
// ============================================================================

/**
 * @brief Initialize door sensor driver
 * @return 0 on success, negative on error
 */
static int init_door_sensor(void) {
    LOG_INFO("Initializing door sensor driver...");
    
    int result = init(); // DoorStateDriver init function
    if (result != 0) {
        LOG_ERROR("Failed to initialize door sensor (error: %d)", result);
        
        // Provide specific error information
        switch (result) {
            case -1:
                LOG_ERROR("WiringPi setup failed - check GPIO permissions");
                break;
            case -2:
                LOG_ERROR("Interrupt setup failed - GPIO pin %d may be in use", DOOR_SENSOR_PIN);
                break;
            case -3:
                LOG_ERROR("Pin mode configuration failed - check hardware connection");
                break;
            case -4:
                LOG_ERROR("Pull-down resistor setup failed - hardware issue");
                break;
            default:
                LOG_ERROR("Unknown door sensor initialization error");
        }
        
        return ERROR_HARDWARE_INIT;
    }
    
    LOG_INFO("Door sensor initialized successfully on GPIO pin %d", DOOR_SENSOR_PIN);
    return 0;
}

/**
 * @brief Initialize device manager
 * @return 0 on success, negative on error
 */
static int init_device_manager(void) {
    LOG_INFO("Initializing device manager...");
    
    int result = device_manager_init(&g_device_manager);
    if (result != 0) {
        LOG_ERROR("Failed to initialize device manager (error: %d)", result);
        return ERROR_GENERIC;
    }
    
    // Start heartbeat monitoring
    result = device_manager_start_heartbeat(&g_device_manager);
    if (result != 0) {
        LOG_ERROR("Failed to start heartbeat monitoring (error: %d)", result);
        device_manager_cleanup(&g_device_manager);
        return ERROR_GENERIC;
    }
    
    LOG_INFO("Device manager initialized - max devices: %d, timeout: %ds", 
             MAX_DEVICES, HEARTBEAT_TIMEOUT);
    return 0;
}

/**
 * @brief Initialize Bluetooth server
 * @return 0 on success, negative on error
 */
static int init_bluetooth_server(void) {
    LOG_INFO("Initializing Bluetooth server...");
    
    int result = bluetooth_server_init(&g_bluetooth_server, &g_device_manager);
    if (result != 0) {
        LOG_ERROR("Failed to initialize Bluetooth server (error: %d)", result);
        return ERROR_GENERIC;
    }
    
    result = bluetooth_server_start(&g_bluetooth_server);
    if (result != 0) {
        LOG_ERROR("Failed to start Bluetooth server (error: %d)", result);
        bluetooth_server_cleanup(&g_bluetooth_server);
        return ERROR_GENERIC;
    }
    
    LOG_INFO("Bluetooth server started on PSM 0x%04X", BLE_PSM);
    return 0;
}

// ============================================================================
// SYSTEM CLEANUP
// ============================================================================

/**
 * @brief Perform complete system cleanup
 */
static void cleanup_system(void) {
    LOG_INFO("Performing system cleanup...");
    
    // Stop and cleanup Bluetooth server
    bluetooth_server_stop(&g_bluetooth_server);
    bluetooth_server_cleanup(&g_bluetooth_server);
    LOG_INFO("Bluetooth server cleaned up");
    
    // Stop and cleanup device manager
    device_manager_stop_heartbeat(&g_device_manager);
    device_manager_cleanup(&g_device_manager);
    LOG_INFO("Device manager cleaned up");
    
    // Note: Door sensor driver doesn't require explicit cleanup
    // as it uses static resources and interrupt handlers
    
    LOG_INFO("System cleanup completed");
}

// ============================================================================
// NOTIFICATION INTEGRATION
// ============================================================================

/**
 * @brief Check and send door close notification if conditions are met
 * @return 0 if notification sent or not needed, negative on error
 */
static int check_door_notification(void) {
    // Check if all devices have disconnected
    if (device_manager_has_devices(&g_device_manager)) {
        return 0; // Still have devices connected
    }
    
    // Get FCM token from last disconnected device
    const char* fcm_token = device_manager_get_last_token(&g_device_manager);
    if (!fcm_token || strlen(fcm_token) == 0) {
        LOG_WARN("No FCM token available for notification");
        return 0;
    }
    
    // Check door state
    DoorState door_state = getDoorState();
    if (door_state != UNLOCKED) {
        if (door_state == LOCKED) {
            LOG_INFO("Door already locked - no reminder needed");
        } else {
            LOG_WARN("Door state unknown - skipping notification");
        }
        return 0;
    }
    
    // All conditions met - send notification
    LOG_INFO("Sending door close reminder - all devices gone, door unlocked");
    
    int result = send_door_close_reminder(fcm_token, FIREBASE_SERVICE_ACCOUNT_PATH);
    if (result == 0) {
        LOG_INFO("Door close reminder sent successfully");
    } else {
        LOG_ERROR("Failed to send door close reminder (error: %d)", result);
    }
    
    return result;
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

/**
 * @brief Main entry point for Door Monitoring System
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, non-zero on error
 */
int main(int argc, char *argv[]) {
    int exit_code = 0;
    
    // Initialize logging system
    if (logger_init() != 0) {
        fprintf(stderr, "Failed to initialize logging system\n");
        return ERROR_GENERIC;
    }
    
    // Log system startup
    log_system_startup(SYSTEM_NAME, SYSTEM_VERSION);
    
    // Check if help was requested
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usage: %s [options]\n", argv[0]);
        printf("Options:\n");
        printf("  -h, --help    Show this help message\n");
        printf("\nDoor Monitoring System v%s\n", SYSTEM_VERSION);
        printf("Monitors door state and BLE device presence for smart notifications.\n");
        printf("\nRequires root privileges for GPIO and Bluetooth access.\n");
        printf("Configure system parameters in config.h\n");
        return 0;
    }
    
    // Install signal handlers
    if (install_signal_handlers() != 0) {
        LOG_ERROR("Failed to install signal handlers");
        exit_code = ERROR_GENERIC;
        goto cleanup;
    }
    
    // Check system requirements
    if (check_system_requirements() != 0) {
        LOG_ERROR("System requirements not met");
        exit_code = ERROR_PRIVILEGES;
        goto cleanup;
    }
    
    // Initialize door sensor driver
    if (init_door_sensor() != 0) {
        LOG_ERROR("Door sensor initialization failed");
        exit_code = ERROR_HARDWARE_INIT;
        goto cleanup;
    }
    
    // Initialize device manager
    if (init_device_manager() != 0) {
        LOG_ERROR("Device manager initialization failed");
        exit_code = ERROR_GENERIC;
        goto cleanup;
    }
    
    // Initialize Bluetooth server
    if (init_bluetooth_server() != 0) {
        LOG_ERROR("Bluetooth server initialization failed");
        exit_code = ERROR_GENERIC;
        goto cleanup;
    }
    
    LOG_INFO("=== %s Ready ===", SYSTEM_NAME);
    LOG_INFO("Monitoring door state and BLE device presence");
    LOG_INFO("Press Ctrl+C to stop");
    
    // Main event loop
    while (g_system_running) {
        int result = bluetooth_server_run_once(&g_bluetooth_server);
        
        if (result < 0) {
            LOG_ERROR("Bluetooth server error: %d", result);
            break;
        }
        
        // Check for notification conditions after each iteration
        // This integrates door state monitoring with device management
        check_door_notification();
        
        // Small delay to prevent busy waiting
        if (result == 1) { // Timeout occurred
            usleep(10000); // 10ms
        }
    }
    
cleanup:
    // System cleanup
    cleanup_system();
    
    // Log system shutdown
    log_system_shutdown(SYSTEM_NAME);
    
    // Cleanup logging system
    logger_cleanup();
    
    return exit_code;
}