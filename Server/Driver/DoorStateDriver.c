/**
 * @file DoorStateDriver.c
 * @brief GPIO driver for door sensor with debounce handling
 * 
 * This driver implements a door sensor monitoring system using WiringPi library.
 * It detects door lock/unlock events through GPIO interrupts with hardware debounce
 * support provided by wiringPiISR2 function.
 * 
 * Hardware Configuration:
 * - Sensor connected to GPIO pin (configured via config.h)
 * - Pull-down resistor enabled (sensor pulls high when triggered)
 * - Rising edge: Door unlocked
 * - Falling edge: Door locked
 * 
 * Debounce Handling:
 * - Hardware debounce: configurable microseconds via config.h
 * - Prevents multiple interrupts from mechanical switch bounce
 */

#include "config.h"
#include "DoorStateDriver.h"

/// Global variable to store current door state (volatile for ISR access)
volatile DoorState boltState = ERROR;

/**
 * @brief Set door state (private function)
 * @param state New door state to set
 * 
 * This function is used internally to update the door state.
 * It's marked as static to prevent external access and ensure
 * state changes only occur through proper interrupt handling.
 */
static void setDoorState(DoorState state)
{
    boltState = state;
}

/**
 * @brief Interrupt callback function for door state changes
 * @param wfiStatus WiringPi interrupt status structure containing edge info and timestamp
 * @param userdata User data pointer (unused in this implementation)
 * 
 * This function is called automatically by the WiringPi library when a GPIO
 * edge is detected on the sensor pin. The hardware debounce prevents multiple
 * calls within the BOUNCE_TIME_US period.
 * 
 * Edge Detection Logic:
 * - INT_EDGE_RISING: Sensor goes HIGH -> Door UNLOCKED
 * - INT_EDGE_FALLING: Sensor goes LOW -> Door LOCKED
 * - Other edges: Set ERROR state
 */
void doorLockedOrUnlocked(struct WPIWfiStatus wfiStatus, void* userdata) 
{
    long long int timenow, diff;
    struct timespec curr;
    
    // Get current timestamp for potential future use
    if (clock_gettime(CLOCK_MONOTONIC, &curr) == -1)
    {
        printf("clock_gettime error: %s\n", strerror(errno));
        setDoorState(ERROR);
        return;
    }
    
    // Convert to microseconds for comparison with wfiStatus.timeStamp_us
    timenow = curr.tv_sec * 1000000LL + curr.tv_nsec/1000L;
    diff = timenow - wfiStatus.timeStamp_us;
    
    // Update door state based on interrupt edge
    if (wfiStatus.edge == INT_EDGE_RISING)
        setDoorState(UNLOCKED);
    else if (wfiStatus.edge == INT_EDGE_FALLING)
        setDoorState(LOCKED);
    else
        setDoorState(ERROR);
}

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
 * The function must be called before using getDoorState().
 * Hardware debounce is handled by wiringPiISR2 with BOUNCE_TIME_US period.
 */
int init(void)
{
    // Initialize WiringPi library
    if (wiringPiSetup() < 0)
    {
        fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
        return -1;
    }
    
    // Setup interrupt service routine with hardware debounce
    if (wiringPiISR2(DOOR_SENSOR_PIN, INT_EDGE_BOTH, &doorLockedOrUnlocked, BOUNCE_TIME_US, NULL) < 0)
    {
        fprintf(stderr, "Unable to setup ISR: %s\n", strerror(errno));
        return -2;
    }
    
    // Configure pin as input
    if (pinMode(DOOR_SENSOR_PIN, INPUT) < 0)
    {
        fprintf(stderr, "Unable to set pin as input: %s\n", strerror(errno));
        return -3;
    }
    
    // Enable pull-down resistor (sensor pulls high when activated)
    if (pullUpDnControl(DOOR_SENSOR_PIN, DOWN) < 0)
    {
        fprintf(stderr, "Unable to setup pull-down control: %s\n", strerror(errno));
        return -4;
    }
    
    // Initialize door state (will be updated by first interrupt)
    setDoorState(ERROR);
    
    return 0;
}

/**
 * @brief Get current door state
 * @return Current door state
 * @retval LOCKED Door is locked
 * @retval UNLOCKED Door is unlocked
 * @retval ERROR Error state or uninitialized
 * 
 * This function returns the current door state as determined by the
 * interrupt service routine. The state is updated automatically when
 * the sensor detects door lock/unlock events.
 * 
 * Note: The function returns ERROR if init() hasn't been called successfully
 * or if there was an error in interrupt processing.
 */
DoorState getDoorState(void)
{
    return boltState;
}