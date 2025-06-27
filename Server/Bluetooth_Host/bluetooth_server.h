/**
 * @file bluetooth_server.h
 * @brief Bluetooth L2CAP Server for BLE Device Communication
 * 
 * This module provides a complete Bluetooth Low Energy server implementation
 * using L2CAP sockets. It handles incoming connections, data reception,
 * and integrates with the device manager for complete BLE device lifecycle
 * management.
 * 
 * Features:
 * - L2CAP socket server with configurable PSM
 * - Multiple concurrent device connections
 * - Event-driven architecture with select() multiplexing
 * - Automatic connection handling and cleanup
 * - Integration with device management system
 * - Thread-safe operations
 */

#ifndef BLUETOOTH_SERVER_H
#define BLUETOOTH_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include "config.h"
#include "logger.h"
#include "device_manager.h"

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @brief Bluetooth server configuration structure
 * 
 * Contains configuration parameters for the Bluetooth server including
 * protocol settings, socket options, and operational parameters.
 */
typedef struct {
    uint16_t psm;                       /// L2CAP Protocol Service Multiplexer
    int max_devices;                    /// Maximum concurrent connections
    int select_timeout_sec;             /// Select timeout in seconds
    int socket_reuse_addr;              /// Enable SO_REUSEADDR option
} BluetoothServerConfig;

/**
 * @brief Bluetooth server state structure
 * 
 * Maintains the current state of the Bluetooth server including
 * socket handles, configuration, and operational status.
 */
typedef struct {
    int server_socket;                  /// Main server socket file descriptor
    BluetoothServerConfig config;       /// Server configuration
    DeviceManager *device_manager;      /// Pointer to device manager
    int running;                        /// Server running state flag
    char receive_buffer[BUFFER_SIZE];   /// Data reception buffer
} BluetoothServer;

// ============================================================================
// SERVER LIFECYCLE
// ============================================================================

/**
 * @brief Initialize Bluetooth server with default configuration
 * @param server Pointer to BluetoothServer structure
 * @param device_manager Pointer to device manager instance
 * @return 0 on success, negative on error
 * 
 * Initializes the Bluetooth server structure with default configuration
 * values from config.h and associates it with a device manager.
 */
int bluetooth_server_init(BluetoothServer *server, DeviceManager *device_manager);

/**
 * @brief Initialize Bluetooth server with custom configuration
 * @param server Pointer to BluetoothServer structure
 * @param device_manager Pointer to device manager instance
 * @param config Pointer to custom configuration
 * @return 0 on success, negative on error
 * 
 * Initializes the Bluetooth server with custom configuration parameters.
 */
int bluetooth_server_init_with_config(BluetoothServer *server, DeviceManager *device_manager, 
                                     const BluetoothServerConfig *config);

/**
 * @brief Create and configure the server socket
 * @param server Pointer to BluetoothServer structure
 * @return 0 on success, negative on error
 * 
 * Creates the L2CAP server socket, configures socket options,
 * binds to the specified PSM, and starts listening for connections.
 */
int bluetooth_server_create_socket(BluetoothServer *server);

/**
 * @brief Start the Bluetooth server
 * @param server Pointer to BluetoothServer structure
 * @return 0 on success, negative on error
 * 
 * Starts the complete Bluetooth server including socket creation
 * and preparation for accepting connections.
 */
int bluetooth_server_start(BluetoothServer *server);

/**
 * @brief Stop the Bluetooth server
 * @param server Pointer to BluetoothServer structure
 * 
 * Gracefully stops the Bluetooth server, closes all connections,
 * and cleans up server resources.
 */
void bluetooth_server_stop(BluetoothServer *server);

/**
 * @brief Clean up server resources
 * @param server Pointer to BluetoothServer structure
 * 
 * Performs complete cleanup of server resources including
 * socket closure and memory deallocation.
 */
void bluetooth_server_cleanup(BluetoothServer *server);

// ============================================================================
// CONNECTION HANDLING
// ============================================================================

/**
 * @brief Accept a new incoming connection
 * @param server Pointer to BluetoothServer structure
 * @return Client socket file descriptor on success, negative on error
 * 
 * Handles incoming BLE connection requests, extracts device MAC address,
 * manages device registration or reconnection, and maintains the active
 * device list with proper synchronization.
 */
int bluetooth_server_accept_connection(BluetoothServer *server);

/**
 * @brief Handle data reception from a connected device
 * @param server Pointer to BluetoothServer structure
 * @param client_socket Client socket file descriptor
 * @return Number of bytes received, 0 on disconnection, negative on error
 * 
 * Receives data from a connected BLE device and forwards it to the
 * device manager for processing. Handles connection errors and
 * disconnection detection.
 */
int bluetooth_server_receive_data(BluetoothServer *server, int client_socket);

/**
 * @brief Handle client disconnection
 * @param server Pointer to BluetoothServer structure
 * @param client_socket Client socket file descriptor
 * @return 0 on success, negative on error
 * 
 * Processes client disconnection events, notifies the device manager,
 * and performs necessary cleanup for the disconnected client.
 */
int bluetooth_server_handle_disconnect(BluetoothServer *server, int client_socket);

// ============================================================================
// MAIN EVENT LOOP
// ============================================================================

/**
 * @brief Main server event loop
 * @param server Pointer to BluetoothServer structure
 * @return 0 on normal termination, negative on error
 * 
 * Implements the main event loop using select() for multiplexed I/O.
 * Handles new connections, data reception from multiple devices,
 * and disconnection detection in a single thread.
 * 
 * This function blocks until the server is stopped via bluetooth_server_stop().
 */
int bluetooth_server_run(BluetoothServer *server);

/**
 * @brief Run server for a single iteration
 * @param server Pointer to BluetoothServer structure
 * @return 0 on success, 1 on timeout, negative on error
 * 
 * Performs a single iteration of the server event loop.
 * Useful for integration with custom event loops or testing.
 */
int bluetooth_server_run_once(BluetoothServer *server);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Get MAC address from client connection
 * @param client_socket Connected client socket
 * @param mac_address Buffer to store MAC address string (must be at least 18 bytes)
 * @return 0 on success, negative on error
 * 
 * Extracts the MAC address of a connected client from the socket
 * connection information and formats it as a string.
 */
int bluetooth_server_get_client_mac(int client_socket, char *mac_address);

/**
 * @brief Check if server is running
 * @param server Pointer to BluetoothServer structure
 * @return 1 if running, 0 if stopped
 */
int bluetooth_server_is_running(BluetoothServer *server);

/**
 * @brief Get server socket file descriptor
 * @param server Pointer to BluetoothServer structure
 * @return Server socket file descriptor, or -1 if not created
 */
int bluetooth_server_get_socket(BluetoothServer *server);

/**
 * @brief Get current server configuration
 * @param server Pointer to BluetoothServer structure
 * @return Pointer to current configuration structure
 */
const BluetoothServerConfig* bluetooth_server_get_config(BluetoothServer *server);

// ============================================================================
// ERROR HANDLING AND DIAGNOSTICS
// ============================================================================

/**
 * @brief Validate Bluetooth server configuration
 * @param config Pointer to configuration to validate
 * @return 0 if valid, negative if invalid
 * 
 * Validates configuration parameters for reasonable values
 * and compatibility with system constraints.
 */
int bluetooth_server_validate_config(const BluetoothServerConfig *config);

/**
 * @brief Check Bluetooth system availability
 * @return 0 if available, negative if not available
 * 
 * Performs basic checks to ensure Bluetooth functionality
 * is available on the system.
 */
int bluetooth_server_check_system(void);

/**
 * @brief Get last error description
 * @return Pointer to error description string
 * 
 * Returns a human-readable description of the last error
 * that occurred in the Bluetooth server module.
 */
const char* bluetooth_server_get_last_error(void);

#endif // BLUETOOTH_SERVER_H