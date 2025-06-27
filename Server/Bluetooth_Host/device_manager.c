/**
 * @file device_manager.c
 * @brief Implementation of BLE Device Management System
 * 
 * This module implements comprehensive BLE device management including
 * device registration, heartbeat monitoring, FCM token storage, and
 * automatic cleanup of disconnected devices with thread-safe operations.
 */

#include "device_manager.h"
#include "fcm_notification.h"
#include "DoorStateDriver.h"

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Initialize a single device structure
 * @param device Pointer to device structure to initialize
 */
static void init_device(Device *device) {
    memset(device->mac_address, 0, sizeof(device->mac_address));
    memset(device->fcm_token, 0, sizeof(device->fcm_token));
    device->socket_fd = -1;
    device->last_heartbeat = 0;
    pthread_mutex_init(&device->device_mutex, NULL);
}

/**
 * @brief Cleanup a single device structure
 * @param device Pointer to device structure to cleanup
 */
static void cleanup_device(Device *device) {
    if (device->socket_fd > 0) {
        close(device->socket_fd);
        device->socket_fd = -1;
    }
    pthread_mutex_destroy(&device->device_mutex);
}

/**
 * @brief Compact device array after removal
 * @param manager Pointer to device manager instance
 * @param removed_index Index of removed device
 */
static void compact_device_array(DeviceManager *manager, int removed_index) {
    for (int i = removed_index; i < manager->device_count - 1; i++) {
        manager->devices[i] = manager->devices[i + 1];
    }
    manager->device_count--;
    
    // Initialize the last slot
    init_device(&manager->devices[manager->device_count]);
}

/**
 * @brief Check notification conditions and send if appropriate
 * @param manager Pointer to device manager instance
 * @return 0 if notification sent or not needed, negative on error
 */
static int check_and_send_notification(DeviceManager *manager) {
    // Check if all devices have disconnected
    if (manager->device_count > 0) {
        return 0; // Still have devices connected
    }
    
    // Check if we have a valid FCM token
    if (strlen(manager->last_disconnected_token) == 0) {
        LOG_WARN("No FCM token available for notification");
        return 0;
    }
    
    // Check door state - only send if unlocked
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
    
    int result = send_door_close_reminder(manager->last_disconnected_token, FIREBASE_SERVICE_ACCOUNT_PATH);
    log_notification_event(result == 0, result, manager->last_disconnected_token);
    
    return result;
}

// ============================================================================
// INITIALIZATION AND CLEANUP
// ============================================================================

int device_manager_init(DeviceManager *manager) {
    if (!manager) {
        LOG_ERROR("Device manager initialization: NULL manager pointer");
        return ERROR_INVALID_PARAM;
    }
    
    LOG_INFO("Initializing device manager...");
    
    // Initialize device array
    for (int i = 0; i < MAX_DEVICES; i++) {
        init_device(&manager->devices[i]);
    }
    
    // Initialize manager state
    manager->device_count = 0;
    memset(manager->last_disconnected_token, 0, sizeof(manager->last_disconnected_token));
    manager->running = 0;
    
    // Initialize manager mutex
    if (pthread_mutex_init(&manager->manager_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize manager mutex");
        return ERROR_GENERIC;
    }
    
    LOG_INFO("Device manager initialized successfully");
    return SUCCESS;
}

void device_manager_cleanup(DeviceManager *manager) {
    if (!manager) {
        return;
    }
    
    LOG_INFO("Cleaning up device manager...");
    
    // Stop if running
    manager->running = 0;
    
    pthread_mutex_lock(&manager->manager_mutex);
    
    // Cleanup all devices
    for (int i = 0; i < MAX_DEVICES; i++) {
        cleanup_device(&manager->devices[i]);
    }
    
    manager->device_count = 0;
    
    pthread_mutex_unlock(&manager->manager_mutex);
    
    // Destroy manager mutex
    pthread_mutex_destroy(&manager->manager_mutex);
    
    LOG_INFO("Device manager cleanup completed");
}

int device_manager_start_heartbeat(DeviceManager *manager) {
    if (!manager) {
        LOG_ERROR("Start heartbeat: NULL manager pointer");
        return ERROR_INVALID_PARAM;
    }
    
    LOG_INFO("Starting heartbeat monitoring thread...");
    
    manager->running = 1;
    
    int result = pthread_create(&manager->heartbeat_thread, NULL, 
                               device_manager_heartbeat_worker, manager);
    if (result != 0) {
        LOG_ERROR("Failed to create heartbeat thread: %s", strerror(result));
        manager->running = 0;
        return ERROR_GENERIC;
    }
    
    LOG_INFO("Heartbeat monitoring thread started");
    return SUCCESS;
}

void device_manager_stop_heartbeat(DeviceManager *manager) {
    if (!manager || !manager->running) {
        return;
    }
    
    LOG_INFO("Stopping heartbeat monitoring thread...");
    
    manager->running = 0;
    
    // Wait for thread to complete
    if (pthread_join(manager->heartbeat_thread, NULL) != 0) {
        LOG_WARN("Failed to join heartbeat thread - forcing cancellation");
        pthread_cancel(manager->heartbeat_thread);
    }
    
    LOG_INFO("Heartbeat monitoring thread stopped");
}

// ============================================================================
// DEVICE SEARCH AND ACCESS
// ============================================================================

Device* device_manager_find_by_socket(DeviceManager *manager, int socket_fd) {
    if (!manager || socket_fd <= 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&manager->manager_mutex);
    
    Device *result = NULL;
    for (int i = 0; i < manager->device_count; i++) {
        if (manager->devices[i].socket_fd == socket_fd) {
            result = &manager->devices[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->manager_mutex);
    return result;
}

Device* device_manager_find_by_mac(DeviceManager *manager, const char* mac_address) {
    if (!manager || !mac_address) {
        return NULL;
    }
    
    pthread_mutex_lock(&manager->manager_mutex);
    
    Device *result = NULL;
    for (int i = 0; i < manager->device_count; i++) {
        if (strcmp(manager->devices[i].mac_address, mac_address) == 0) {
            result = &manager->devices[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->manager_mutex);
    return result;
}

int device_manager_get_count(DeviceManager *manager) {
    if (!manager) {
        return 0;
    }
    
    pthread_mutex_lock(&manager->manager_mutex);
    int count = manager->device_count;
    pthread_mutex_unlock(&manager->manager_mutex);
    
    return count;
}

int device_manager_has_capacity(DeviceManager *manager) {
    if (!manager) {
        return 0;
    }
    
    return device_manager_get_count(manager) < MAX_DEVICES;
}

// ============================================================================
// DEVICE LIFECYCLE MANAGEMENT
// ============================================================================

Device* device_manager_add_device(DeviceManager *manager, const char* mac_address, int socket_fd) {
    if (!manager || !mac_address || socket_fd <= 0) {
        LOG_ERROR("Add device: Invalid parameters");
        return NULL;
    }
    
    pthread_mutex_lock(&manager->manager_mutex);
    
    // Check capacity
    if (manager->device_count >= MAX_DEVICES) {
        LOG_ERROR("Cannot add device - maximum capacity reached (%d/%d)", 
                 manager->device_count, MAX_DEVICES);
        pthread_mutex_unlock(&manager->manager_mutex);
        return NULL;
    }
    
    // Get next available device slot
    Device *device = &manager->devices[manager->device_count];
    
    pthread_mutex_lock(&device->device_mutex);
    
    // Initialize device
    strncpy(device->mac_address, mac_address, sizeof(device->mac_address) - 1);
    device->mac_address[sizeof(device->mac_address) - 1] = '\0';
    device->socket_fd = socket_fd;
    device->last_heartbeat = time(NULL);
    memset(device->fcm_token, 0, sizeof(device->fcm_token));
    
    manager->device_count++;
    
    pthread_mutex_unlock(&device->device_mutex);
    pthread_mutex_unlock(&manager->manager_mutex);
    
    log_device_connect(mac_address, manager->device_count);
    
    return device;
}

int device_manager_remove_device(DeviceManager *manager, Device* device) {
    if (!manager || !device) {
        LOG_ERROR("Remove device: Invalid parameters");
        return ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->manager_mutex);
    
    // Find device index
    int device_index = -1;
    for (int i = 0; i < manager->device_count; i++) {
        if (&manager->devices[i] == device) {
            device_index = i;
            break;
        }
    }
    
    if (device_index == -1) {
        LOG_ERROR("Device not found in manager");
        pthread_mutex_unlock(&manager->manager_mutex);
        return ERROR_GENERIC;
    }
    
    pthread_mutex_lock(&device->device_mutex);
    
    // Save FCM token for potential notification
    if (strlen(device->fcm_token) > 0) {
        strncpy(manager->last_disconnected_token, device->fcm_token, 
                sizeof(manager->last_disconnected_token) - 1);
        manager->last_disconnected_token[sizeof(manager->last_disconnected_token) - 1] = '\0';
    }
    
    // Log disconnection
    log_device_disconnect(device->mac_address, manager->device_count - 1, "removed");
    
    // Close socket
    if (device->socket_fd > 0) {
        close(device->socket_fd);
        device->socket_fd = -1;
    }
    
    pthread_mutex_unlock(&device->device_mutex);
    
    // Compact array
    compact_device_array(manager, device_index);
    
    // Check for notification conditions
    check_and_send_notification(manager);
    
    pthread_mutex_unlock(&manager->manager_mutex);
    
    return SUCCESS;
}

void device_manager_update_heartbeat(Device* device) {
    if (!device) {
        return;
    }
    
    pthread_mutex_lock(&device->device_mutex);
    device->last_heartbeat = time(NULL);
    pthread_mutex_unlock(&device->device_mutex);
}

int device_manager_reconnect_device(DeviceManager *manager, Device* existing_device, int new_socket_fd) {
    if (!manager || !existing_device || new_socket_fd <= 0) {
        LOG_ERROR("Reconnect device: Invalid parameters");
        return ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&existing_device->device_mutex);
    
    // Close old socket if open
    if (existing_device->socket_fd > 0) {
        close(existing_device->socket_fd);
    }
    
    // Update with new socket
    existing_device->socket_fd = new_socket_fd;
    existing_device->last_heartbeat = time(NULL);
    
    pthread_mutex_unlock(&existing_device->device_mutex);
    
    LOG_INFO("Device reconnected: %s", existing_device->mac_address);
    
    return SUCCESS;
}

// ============================================================================
// DATA PROCESSING
// ============================================================================

int device_manager_process_data(DeviceManager *manager, int socket_fd, const char *data, size_t length) {
    if (!manager || !data || length == 0) {
        return ERROR_INVALID_PARAM;
    }
    
    Device *device = device_manager_find_by_socket(manager, socket_fd);
    if (!device) {
        LOG_WARN("Received data from unknown device (socket %d)", socket_fd);
        return ERROR_GENERIC;
    }
    
    pthread_mutex_lock(&device->device_mutex);
    
    // Prepare JSON buffer
    char json_buffer[BUFFER_SIZE];
    size_t copy_length = (length >= BUFFER_SIZE) ? BUFFER_SIZE - 1 : length;
    
    strncpy(json_buffer, data, copy_length);
    json_buffer[copy_length] = '\0';
    
    // Parse JSON
    json_object *root = json_tokener_parse(json_buffer);
    if (root) {
        json_object *fcm_token_obj;
        
        // Extract FCM token
        if (json_object_object_get_ex(root, "fcm_token", &fcm_token_obj)) {
            const char *fcm_token = json_object_get_string(fcm_token_obj);
            if (fcm_token && strlen(fcm_token) >= MIN_FCM_TOKEN_LENGTH) {
                strncpy(device->fcm_token, fcm_token, TOKEN_SIZE - 1);
                device->fcm_token[TOKEN_SIZE - 1] = '\0';
                LOG_INFO("FCM token updated for device: %s", device->mac_address);
            } else {
                LOG_WARN("Invalid FCM token received from %s (length: %zu)", 
                        device->mac_address, fcm_token ? strlen(fcm_token) : 0);
            }
        }
        
        json_object_put(root);
    } else {
        LOG_WARN("Invalid JSON received from %s", device->mac_address);
    }
    
    // Update heartbeat
    device->last_heartbeat = time(NULL);
    
    pthread_mutex_unlock(&device->device_mutex);
    
    return SUCCESS;
}

int device_manager_handle_disconnect(DeviceManager *manager, int socket_fd) {
    if (!manager) {
        return ERROR_INVALID_PARAM;
    }
    
    Device *device = device_manager_find_by_socket(manager, socket_fd);
    if (!device) {
        LOG_WARN("Disconnect from unknown device (socket %d)", socket_fd);
        return ERROR_GENERIC;
    }
    
    return device_manager_remove_device(manager, device);
}

// ============================================================================
// STATUS AND MONITORING
// ============================================================================

void device_manager_print_status(DeviceManager *manager) {
    if (!manager) {
        return;
    }
    
    pthread_mutex_lock(&manager->manager_mutex);
    
    printf("\n📊 Connected Devices: %d/%d\n", manager->device_count, MAX_DEVICES);
    printf("┌─────────────────────┬─────────────────────┬─────────────┐\n");
    printf("│ MAC Address         │ FCM Token Preview   │ Last Beat   │\n");
    printf("├─────────────────────┼─────────────────────┼─────────────┤\n");
    
    time_t now = time(NULL);
    for (int i = 0; i < manager->device_count; i++) {
        Device *device = &manager->devices[i];
        char token_preview[22] = "Waiting...";
        char heartbeat_str[12] = "Never";
        
        pthread_mutex_lock(&device->device_mutex);
        
        if (strlen(device->fcm_token) > 0) {
            snprintf(token_preview, sizeof(token_preview), "%.15s...", device->fcm_token);
        }
        
        if (device->last_heartbeat > 0) {
            int seconds_ago = (int)(now - device->last_heartbeat);
            snprintf(heartbeat_str, sizeof(heartbeat_str), "%ds ago", seconds_ago);
        }
        
        printf("│ %-19s │ %-19s │ %-11s │\n", 
               device->mac_address, token_preview, heartbeat_str);
        
        pthread_mutex_unlock(&device->device_mutex);
    }
    
    printf("└─────────────────────┴─────────────────────┴─────────────┘\n");
    
    if (strlen(manager->last_disconnected_token) > 0) {
        printf("Last disconnected token: %.20s...\n", manager->last_disconnected_token);
    }
    
    printf("\n");
    
    pthread_mutex_unlock(&manager->manager_mutex);
}

const char* device_manager_get_last_token(DeviceManager *manager) {
    if (!manager) {
        return NULL;
    }
    
    pthread_mutex_lock(&manager->manager_mutex);
    const char *token = (strlen(manager->last_disconnected_token) > 0) ? 
                       manager->last_disconnected_token : NULL;
    pthread_mutex_unlock(&manager->manager_mutex);
    
    return token;
}

int device_manager_has_devices(DeviceManager *manager) {
    return device_manager_get_count(manager) > 0;
}

// ============================================================================
// HEARTBEAT MONITORING
// ============================================================================

void* device_manager_heartbeat_worker(void* arg) {
    DeviceManager *manager = (DeviceManager*)arg;
    if (!manager) {
        LOG_ERROR("Heartbeat worker: NULL manager");
        return NULL;
    }
    
    LOG_INFO("Heartbeat monitoring started");
    
    while (manager->running) {
        sleep(HEARTBEAT_CHECK_INTERVAL);
        
        if (!manager->running) {
            break;
        }
        
        int removed_count = device_manager_check_timeouts(manager);
        if (removed_count > 0) {
            device_manager_print_status(manager);
        }
    }
    
    LOG_INFO("Heartbeat monitoring stopped");
    return NULL;
}

int device_manager_check_timeouts(DeviceManager *manager) {
    if (!manager) {
        return 0;
    }
    
    time_t now = time(NULL);
    int removed_count = 0;
    
    pthread_mutex_lock(&manager->manager_mutex);
    
    // Check from end to beginning to avoid index shifting issues
    for (int i = manager->device_count - 1; i >= 0; i--) {
        Device *device = &manager->devices[i];
        
        pthread_mutex_lock(&device->device_mutex);
        
        if (now - device->last_heartbeat > HEARTBEAT_TIMEOUT) {
            LOG_INFO("Device timeout: %s (last seen %ld seconds ago)", 
                    device->mac_address, now - device->last_heartbeat);
            
            // Save FCM token
            if (strlen(device->fcm_token) > 0) {
                strncpy(manager->last_disconnected_token, device->fcm_token,
                       sizeof(manager->last_disconnected_token) - 1);
                manager->last_disconnected_token[sizeof(manager->last_disconnected_token) - 1] = '\0';
            }
            
            // Log disconnection
            log_device_disconnect(device->mac_address, manager->device_count - 1, "timeout");
            
            // Close socket
            if (device->socket_fd > 0) {
                close(device->socket_fd);
                device->socket_fd = -1;
            }
            
            pthread_mutex_unlock(&device->device_mutex);
            
            // Remove from array
            compact_device_array(manager, i);
            removed_count++;
        } else {
            pthread_mutex_unlock(&device->device_mutex);
        }
    }
    
    // Check for notification after all timeouts processed
    if (removed_count > 0) {
        check_and_send_notification(manager);
    }
    
    pthread_mutex_unlock(&manager->manager_mutex);
    
    return removed_count;
}