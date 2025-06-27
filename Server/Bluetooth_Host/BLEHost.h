/**
 * @file BLEHost.h
 * @brief Door Monitoring System - Main Header File
 * 
 * This is the main header file for the Door Monitoring System that combines
 * Bluetooth Low Energy device tracking with GPIO door sensor monitoring to
 * provide intelligent door-close reminders via Firebase Cloud Messaging.
 * 
 * The system has been refactored into modular components for better
 * maintainability, testing, and code organization:
 * 
 * - logger: Centralized logging system
 * - device_manager: BLE device tracking and heartbeat monitoring
 * - bluetooth_server: L2CAP Bluetooth server implementation
 * - main: System initialization and coordination
 * - config: Centralized configuration management
 * 
 * Usage:
 * Include this header to access all system components. The main entry
 * point is in main.c which coordinates all subsystems.
 */

#ifndef BLEHOST_H
#define BLEHOST_H

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================

#include "config.h"

// ============================================================================
// CORE SYSTEM MODULES
// ============================================================================

#include "logger.h"
#include "device_manager.h"
#include "bluetooth_server.h"

// ============================================================================
// EXTERNAL DEPENDENCIES
// ============================================================================

#include "DoorStateDriver.h"
#include "fcm_notification.h"

// ============================================================================
// SYSTEM INFORMATION
// ============================================================================

#define SYSTEM_NAME "Door Monitoring System"
#define SYSTEM_VERSION "1.0.0"
#define SYSTEM_DESCRIPTION "BLE Device Tracking with Smart Door Notifications"

// ============================================================================
// INTEGRATION FUNCTIONS
// ============================================================================

/**
 * @brief Check and send door notification if conditions are met
 * @param device_manager Pointer to device manager instance
 * @return 0 if notification sent or not needed, negative on error
 * 
 * Integrates door sensor state with device presence to determine
 * if a door-close reminder notification should be sent.
 * 
 * Conditions for notification:
 * 1. No BLE devices currently connected
 * 2. Door sensor indicates UNLOCKED state
 * 3. Valid FCM token available from last disconnected device
 */
int check_door_notification(DeviceManager *device_manager);

/**
 * @brief Initialize complete door monitoring system
 * @param device_manager Pointer to device manager to initialize
 * @param bluetooth_server Pointer to Bluetooth server to initialize
 * @return 0 on success, negative on error
 * 
 * Performs complete system initialization including:
 * - Logging system setup
 * - Door sensor driver initialization
 * - Device manager setup with heartbeat monitoring
 * - Bluetooth server creation and startup
 * - Signal handler installation
 */
int system_init(DeviceManager *device_manager, BluetoothServer *bluetooth_server);

/**
 * @brief Cleanup complete door monitoring system
 * @param device_manager Pointer to device manager to cleanup
 * @param bluetooth_server Pointer to Bluetooth server to cleanup
 * 
 * Performs complete system cleanup including:
 * - Bluetooth server shutdown and cleanup
 * - Device manager shutdown and cleanup
 * - Logging system cleanup
 * - Resource deallocation
 */
void system_cleanup(DeviceManager *device_manager, BluetoothServer *bluetooth_server);

/**
 * @brief Check system requirements and privileges
 * @return 0 if requirements met, negative if not
 * 
 * Validates system requirements including:
 * - Root privileges for GPIO and Bluetooth access
 * - Firebase service account file accessibility
 * - Bluetooth system availability
 * - GPIO hardware availability
 */
int check_system_requirements(void);

// ============================================================================
// SYSTEM STATUS AND MONITORING
// ============================================================================

/**
 * @brief Display comprehensive system status
 * @param device_manager Pointer to device manager instance
 * @param bluetooth_server Pointer to Bluetooth server instance
 * 
 * Displays detailed status information including:
 * - Connected device count and details
 * - Door sensor current state
 * - Bluetooth server status
 * - System configuration summary
 */
void display_system_status(DeviceManager *device_manager, BluetoothServer *bluetooth_server);

/**
 * @brief Get system health status
 * @param device_manager Pointer to device manager instance
 * @param bluetooth_server Pointer to Bluetooth server instance
 * @return 1 if system healthy, 0 if issues detected
 * 
 * Performs health checks on all system components and returns
 * overall system health status.
 */
int get_system_health(DeviceManager *device_manager, BluetoothServer *bluetooth_server);

// ============================================================================
// LEGACY COMPATIBILITY
// ============================================================================

/**
 * @brief Legacy compatibility types
 * 
 * These types are maintained for backward compatibility with existing
 * code that may reference the original monolithic BLEHost structure.
 */
typedef DeviceManager BLEDeviceManager;
typedef BluetoothServer BLEServer;

/**
 * @brief Legacy device structure alias
 * 
 * Maintains compatibility with code expecting the original Device structure.
 */
typedef Device BLEDevice;

// ============================================================================
// INTEGRATION NOTES FOR REFACTORED ARCHITECTURE
// ============================================================================

/**
 * @note Modular Architecture Benefits
 * The refactored architecture provides several advantages:
 * 
 * 1. **Single Responsibility**: Each module has one clear purpose
 * 2. **Testability**: Modules can be unit tested independently
 * 3. **Maintainability**: Changes are localized to specific modules
 * 4. **Reusability**: Modules can be reused in other projects
 * 5. **Debugging**: Issues are easier to isolate and fix
 * 
 * @note Module Dependencies
 * - logger: No dependencies (can be used standalone)
 * - device_manager: Depends on logger and config
 * - bluetooth_server: Depends on logger, device_manager, and config
 * - main: Coordinates all modules and external dependencies
 * 
 * @note Thread Safety
 * All modules implement proper thread safety:
 * - logger: Thread-safe logging with mutex protection
 * - device_manager: Thread-safe device operations and heartbeat monitoring
 * - bluetooth_server: Thread-safe socket operations
 * 
 * @note Configuration Management
 * All configuration is centralized in config.h:
 * - No scattered #define statements
 * - Single point of configuration changes
 * - Consistent parameter usage across modules
 * 
 * @note Error Handling
 * Comprehensive error handling throughout:
 * - Detailed error codes and messages
 * - Graceful degradation on component failures
 * - Proper resource cleanup on errors
 * - Logging of all error conditions
 */

#endif // BLEHOST_H