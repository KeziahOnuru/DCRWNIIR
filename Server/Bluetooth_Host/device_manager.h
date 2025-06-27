/**
 * @file device_manager.h
 * @brief BLE Device Management System
 * 
 * This module handles the management of connected BLE devices including
 * device registration, heartbeat monitoring, FCM token storage, and
 * automatic cleanup of disconnected devices.
 * 
 * Features:
 * - Multi-device support with configurable limits
 * - Thread-safe device operations
 * - Heartbeat-based presence detection
 * - Automatic timeout handling
 * - FCM token management
 * - Device reconnection support
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <json-c/json.h>

#include "config.h"
#include "logger.h"

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @brief Device structure for BLE device management
 * 
 * Contains essential information for tracking connected BLE devices,
 * including their identification, communication socket, FCM token,
 * and heartbeat status for presence detection.
 */
typedef struct {
    char mac_address[18];               /// Device MAC address (unique identifier)
    char fcm_token[TOKEN_SIZE];         /// Firebase Cloud Messaging token
    int socket_fd;                      /// L2CAP socket file descriptor
    time_t last_heartbeat;              /// Timestamp of last received data
    pthread_mutex_t device_mutex;       /// Thread-safe access protection
} Device;

/**
 * @brief Device manager structure
 * 
 * Central management structure that maintains the list of connected devices,
 * server state, and coordination mechanisms for the BLE host system.
 */
typedef struct {
    Device devices[MAX_DEVICES];           /// Array of managed devices
    int device_count;                      /// Current number of connected devices
    char last_disconnected_token[TOKEN_SIZE]; /// FCM token of last disconnected device
    int running;                           /// Manager running state flag
    pthread_mutex_t manager_mutex;         /// Thread-safe manager access
    pthread_t heartbeat_thread;            /// Heartbeat monitoring thread handle
} DeviceManager;

// ============================================================================
// INITIALIZATION AND CLEANUP
// ============================================================================

/**
 * @brief Initialize the device manager
 * @return 0 on success, negative on error
 * 
 * Sets up the device manager including mutex initialization,
 * device array preparation, and heartbeat thread startup.
 */
int device_manager_init(DeviceManager *manager);

/**
 * @brief Cleanup device manager resources
 * @param manager Pointer to device manager instance
 * 
 * Properly shuts down the device manager including thread termination,
 * device cleanup, and mutex destruction.
 */
void device_manager_cleanup(DeviceManager *manager);

/**
 * @brief Start heartbeat monitoring thread
 * @param manager Pointer to device manager instance
 * @return 0 on success, negative on error
 * 
 * Starts the background thread that monitors device heartbeats
 * and automatically removes unresponsive devices.
 */
int device_manager_start_heartbeat(DeviceManager *manager);

/**
 * @brief Stop heartbeat monitoring thread
 * @param manager Pointer to device manager instance
 * 
 * Gracefully stops the heartbeat monitoring thread and waits
 * for it to complete.
 */
void device_manager_stop_heartbeat(DeviceManager *manager);

// ============================================================================
// DEVICE SEARCH AND ACCESS
// ============================================================================

/**
 * @brief Find device by socket file descriptor
 * @param manager Pointer to device manager instance
 * @param socket_fd Socket file descriptor to search for
 * @return Pointer to Device structure, or NULL if not found
 * 
 * Thread-safe search function for locating a device based on its
 * active socket connection. Used during data reception and disconnection handling.
 */
Device* device_manager_find_by_socket(DeviceManager *manager, int socket_fd);

/**
 * @brief Find device by MAC address
 * @param manager Pointer to device manager instance
 * @param mac_address MAC address string to search for
 * @return Pointer to Device structure, or NULL if not found
 * 
 * Thread-safe search function for locating a device based on its
 * MAC address. Used for handling device reconnections.
 */
Device* device_manager_find_by_mac(DeviceManager *manager, const char* mac_address);

/**
 * @brief Get current device count
 * @param manager Pointer to device manager instance
 * @return Number of currently connected devices
 * 
 * Thread-safe function to get the current number of connected devices.
 */
int device_manager_get_count(DeviceManager *manager);

/**
 * @brief Check if device manager has capacity for new devices
 * @param manager Pointer to device manager instance
 * @return 1 if capacity available, 0 if full
 */
int device_manager_has_capacity(DeviceManager *manager);

// ============================================================================
// DEVICE LIFECYCLE MANAGEMENT
// ============================================================================

/**
 * @brief Add a new device to the manager
 * @param manager Pointer to device manager instance
 * @param mac_address MAC address of the new device
 * @param socket_fd Socket file descriptor for the device
 * @return Pointer to created Device structure, or NULL on error
 * 
 * Thread-safe function to add a new device to the management system.
 * Initializes device structure and assigns it to the next available slot.
 */
Device* device_manager_add_device(DeviceManager *manager, const char* mac_address, int socket_fd);

/**
 * @brief Remove a device from the manager
 * @param manager Pointer to device manager instance
 * @param device Pointer to device to remove
 * @return 0 on success, negative on error
 * 
 * Thread-safe function to remove a device from the management system.
 * Properly cleans up device resources and compacts the device array.
 */
int device_manager_remove_device(DeviceManager *manager, Device* device);

/**
 * @brief Update device heartbeat timestamp
 * @param device Pointer to device structure
 * 
 * Thread-safe function to update the last heartbeat timestamp
 * for a device. Called whenever data is received from the device.
 */
void device_manager_update_heartbeat(Device* device);

/**
 * @brief Handle device reconnection
 * @param manager Pointer to device manager instance
 * @param existing_device Pointer to existing device structure
 * @param new_socket_fd New socket file descriptor
 * @return 0 on success, negative on error
 * 
 * Updates an existing device's socket information for reconnection scenarios.
 */
int device_manager_reconnect_device(DeviceManager *manager, Device* existing_device, int new_socket_fd);

// ============================================================================
// DATA PROCESSING
// ============================================================================

/**
 * @brief Process received data from a device
 * @param manager Pointer to device manager instance
 * @param socket_fd Socket file descriptor of sending device
 * @param data Pointer to received data buffer
 * @param length Number of bytes received
 * @return 0 on success, negative on error
 * 
 * Processes JSON data received from BLE devices, extracting FCM tokens
 * and updating heartbeat timestamps. Validates token format and
 * updates device records accordingly.
 */
int device_manager_process_data(DeviceManager *manager, int socket_fd, const char *data, size_t length);

/**
 * @brief Handle device disconnection
 * @param manager Pointer to device manager instance
 * @param socket_fd Socket file descriptor of disconnected device
 * @return 0 on success, negative on error
 * 
 * Manages device removal from the active list, saves FCM token for
 * potential notifications, and triggers notification logic when appropriate.
 */
int device_manager_handle_disconnect(DeviceManager *manager, int socket_fd);

// ============================================================================
// STATUS AND MONITORING
// ============================================================================

/**
 * @brief Display current device status
 * @param manager Pointer to device manager instance
 * 
 * Thread-safe function that prints a formatted table showing
 * all connected devices with their MAC addresses and FCM token previews.
 * Used for monitoring and debugging purposes.
 */
void device_manager_print_status(DeviceManager *manager);

/**
 * @brief Get last disconnected device token
 * @param manager Pointer to device manager instance
 * @return Pointer to last disconnected token string, or NULL if none
 * 
 * Returns the FCM token of the last device that disconnected.
 * Used for sending notifications when all devices have left.
 */
const char* device_manager_get_last_token(DeviceManager *manager);

/**
 * @brief Check if any devices are currently connected
 * @param manager Pointer to device manager instance
 * @return 1 if devices connected, 0 if none
 */
int device_manager_has_devices(DeviceManager *manager);

// ============================================================================
// HEARTBEAT MONITORING
// ============================================================================

/**
 * @brief Heartbeat monitoring worker thread function
 * @param arg Pointer to DeviceManager instance
 * @return NULL on thread completion
 * 
 * Background thread that periodically checks all connected devices
 * for heartbeat timeouts. Automatically removes unresponsive devices
 * and triggers notifications when appropriate.
 * 
 * Internal function - do not call directly.
 */
void* device_manager_heartbeat_worker(void* arg);

/**
 * @brief Check for device timeouts and remove expired devices
 * @param manager Pointer to device manager instance
 * @return Number of devices removed due to timeout
 * 
 * Scans all devices for heartbeat timeouts and removes expired ones.
 * Called periodically by the heartbeat worker thread.
 */
int device_manager_check_timeouts(DeviceManager *manager);

#endif // DEVICE_MANAGER_H