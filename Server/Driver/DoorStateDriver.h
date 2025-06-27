/**
 * @file DoorStateDriver.h
 * @brief GPIO driver for door sensor with debounce handling - Header file
 * 
 * This header defines the interface for a door sensor monitoring system using
 * WiringPi library. It provides functions to initialize the GPIO driver and
 * retrieve the current door state with hardware debounce support.
 * 
 * Hardware Configuration:
 * - Sensor connected to GPIO pin (configurable via config.h)
 * - Pull-down resistor enabled (sensor pulls high when triggered)
 * - Rising edge: Door unlocked
 * - Falling edge: Door locked
 * - Hardware debounce: configurable microseconds
 * 
 * Usage:
 * 1. Call init() to initialize the GPIO driver
 * 2. Use getDoorState() to read current door state
 * 3. The state is updated automatically via interrupts
 */

#ifndef DOORSTATEDRIVER_H
#define DOORSTATEDRIVER_H

#include <wiringPi.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

// Configuration imports
#include "config.h"

/**
 * @brief Door state enumeration
 * 
 * Defines the possible states of the door lock mechanism.
 * Used by both the interrupt handler and the state query function.
 */
typedef enum {
    LOCKED   = 0,     /// Door is locked (sensor LOW)
    UNLOCKED = 1,    /// Door is unlocked (sensor HIGH)
    ERROR    = -1    /// Error state or uninitialized
} DoorState;

/**
 * @brief Initialize the GPIO driver for door sensor
 * @return 0 on success, negative value on error
 * @retval 0 Success - GPIO driver initialized
 * @retval -1 WiringPi setup failed
 * @retval -2 ISR setup failed
 * @retval -3 Pin mode configuration failed
 * @retval -4 Pull control configuration failed
 * 
 * This function performs complete initialization of the door sensor:
 * 1. Initialize WiringPi library
 * 2. Setup interrupt service routine with debounce
 * 3. Configure pin as input
 * 4. Enable pull-down resistor
 * 
 * Must be called before using getDoorState().
 */
int init(void);

/**
 * @brief Get current door state
 * @return Current door state
 * @retval LOCKED Door is locked
 * @retval UNLOCKED Door is unlocked
 * @retval ERROR Error state or uninitialized
 * 
 * Returns the current door state as determined by the interrupt service routine.
 * The state is updated automatically when the sensor detects door events.
 * 
 * Note: Returns ERROR if init() hasn't been called successfully.
 */
DoorState getDoorState(void);

/**
 * @brief Interrupt callback function for door state changes
 * @param wfiStatus WiringPi interrupt status structure containing edge info and timestamp
 * @param userdata User data pointer (unused in this implementation)
 * 
 * This function is called automatically by the WiringPi library when a GPIO
 * edge is detected. It's declared here for completeness but should not be
 * called directly by user code.
 * 
 * Internal use only - registered automatically by init().
 */
void doorLockedOrUnlocked(struct WPIWfiStatus wfiStatus, void* userdata);

#endif // DOORSTATEDRIVER_H