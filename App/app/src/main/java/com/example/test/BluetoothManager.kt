package com.example.test

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.util.Log
import androidx.core.app.ActivityCompat
import kotlinx.coroutines.*
import java.io.IOException
import java.util.*

class BluetoothManager(private val context: Context) {

    private val TAG = "BluetoothManager"
    private val TARGET_NAME = "DCRWNIIR"
    private val RFCOMM_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    // Alternative UUIDs to test
    private val RFCOMM_UUID_ALTERNATIVE = UUID.fromString("00001105-0000-1000-8000-00805F9B34FB") // OBEX
    private val RFCOMM_UUID_SPP = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB") // Serial Port Profile

    // Optimized configuration for battery saving
    private val SCAN_INTERVAL_MS = 60_000L // 1 minute instead of 5 seconds
    private val SCAN_DURATION_MS = 12_000L // 12 seconds of active scan
    private val MAX_CONNECTION_RETRIES = 3
    private val CONNECTION_TIMEOUT_MS = 30_000L // 30 seconds timeout

    private var bluetoothAdapter: BluetoothAdapter? = null
    private var isScanning = false
    private var scanJob: Job? = null
    private var connectionRetries = 0
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    var onDeviceConnected: ((BluetoothDevice) -> Unit)? = null
    var onConnectionFailed: ((String) -> Unit)? = null

    private val bluetoothReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                BluetoothDevice.ACTION_FOUND -> {
                    val device: BluetoothDevice? = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                    device?.let { handleDeviceFound(it) }
                }
                BluetoothAdapter.ACTION_DISCOVERY_FINISHED -> {
                    Log.d(TAG, "Scan completed")
                    stopActiveScanning()
                    scheduleNextScan()
                }
                BluetoothAdapter.ACTION_STATE_CHANGED -> {
                    val state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1)
                    if (state == BluetoothAdapter.STATE_ON) {
                        startOptimizedScanning()
                    } else if (state == BluetoothAdapter.STATE_OFF) {
                        stopAllScanning()
                    }
                }
                BluetoothDevice.ACTION_BOND_STATE_CHANGED -> {
                    val device: BluetoothDevice? = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                    val bondState = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, -1)
                    device?.let { handleBondStateChanged(it, bondState) }
                }
            }
        }
    }

    fun initialize() {
        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()

        val filter = IntentFilter().apply {
            addAction(BluetoothDevice.ACTION_FOUND)
            addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED)
            addAction(BluetoothAdapter.ACTION_STATE_CHANGED)
            addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
        }

        try {
            context.registerReceiver(bluetoothReceiver, filter)
        } catch (e: Exception) {
            Log.e(TAG, "Error registering receiver", e)
        }

        if (bluetoothAdapter?.isEnabled == true) {
            startOptimizedScanning()
        }
    }

    fun cleanup() {
        stopAllScanning()
        scope.cancel()

        try {
            context.unregisterReceiver(bluetoothReceiver)
        } catch (e: Exception) {
            Log.e(TAG, "Error unregistering receiver", e)
        }
    }

    private fun startOptimizedScanning() {
        if (!hasBluetoothPermissions()) {
            Log.e(TAG, "Missing Bluetooth permissions")
            return
        }

        // Cancel previous scan
        scanJob?.cancel()

        // New optimized scan cycle
        scanJob = scope.launch {
            while (isActive) {
                try {
                    Log.d(TAG, "Starting optimized scan cycle")
                    startActiveScanning()

                    // Wait for scan to finish or timeout
                    delay(SCAN_DURATION_MS)

                    // Stop active scan if still running
                    if (isScanning) {
                        stopActiveScanning()
                    }

                    // Wait before next cycle
                    Log.d(TAG, "Waiting ${SCAN_INTERVAL_MS / 1000}s before next scan")
                    delay(SCAN_INTERVAL_MS)

                } catch (e: CancellationException) {
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Error during scan cycle", e)
                    delay(30_000) // Wait 30s on error
                }
            }
        }
    }

    private fun startActiveScanning() {
        bluetoothAdapter?.let { adapter ->
            if (!adapter.isEnabled) {
                Log.w(TAG, "Bluetooth disabled")
                return
            }

            if (isScanning) {
                adapter.cancelDiscovery()
            }

            Log.d(TAG, "Starting active scan...")
            isScanning = adapter.startDiscovery()
            Log.d(TAG, "startDiscovery() launched? $isScanning")

            if (!isScanning) {
                Log.e(TAG, "Cannot start scan")
            }
        }
    }

    private fun stopActiveScanning() {
        bluetoothAdapter?.let { adapter ->
            if (isScanning && hasBluetoothPermissions()) {
                adapter.cancelDiscovery()
                isScanning = false
                Log.d(TAG, "Active scan stopped")
            }
        }
    }

    private fun stopAllScanning() {
        scanJob?.cancel()
        stopActiveScanning()
    }

    private fun scheduleNextScan() {
        // Next scan is already scheduled in main loop
        Log.d(TAG, "Next scan scheduled in ${SCAN_INTERVAL_MS / 1000}s")
    }

    private fun handleDeviceFound(device: BluetoothDevice) {
        if (!hasBluetoothPermissions()) return

        val deviceName = try { device.name } catch (e: SecurityException) { "Unknown" }
        val deviceAddress = device.address

        // Log ALL found devices for debug
        Log.d(TAG, "Device found: '$deviceName' ($deviceAddress)")

        // Check target name
        if (deviceName?.contains(TARGET_NAME, ignoreCase = true) == true) {
            Log.i(TAG, "✓ Target server found: $deviceName - $deviceAddress")
            stopActiveScanning() // Stop immediately to save battery

            when (device.bondState) {
                BluetoothDevice.BOND_BONDED -> {
                    Log.d(TAG, "Device already paired")
                    attemptConnection(device)
                }
                BluetoothDevice.BOND_NONE -> {
                    Log.d(TAG, "Starting pairing")
                    startPairing(device)
                }
                BluetoothDevice.BOND_BONDING -> {
                    Log.d(TAG, "Pairing in progress")
                }
            }
        } else {
            Log.v(TAG, "Device ignored: '$deviceName' doesn't match '$TARGET_NAME'")
        }
    }

    private fun startPairing(device: BluetoothDevice) {
        if (!hasBluetoothPermissions()) return

        scope.launch {
            try {
                Log.d(TAG, "Attempting pairing with ${device.address}")
                val success = device.createBond()
                if (!success) {
                    Log.e(TAG, "Pairing request failed")
                    onConnectionFailed?.invoke("Pairing failed")
                }
            } catch (e: SecurityException) {
                Log.e(TAG, "Missing permission for pairing", e)
                onConnectionFailed?.invoke("Missing permission")
            }
        }
    }

    private fun handleBondStateChanged(device: BluetoothDevice, bondState: Int) {
        if (!hasBluetoothPermissions()) return

        val deviceName = try { device.name } catch (e: SecurityException) { "Unknown" }

        if (deviceName?.equals(TARGET_NAME, ignoreCase = true) != true) return

        when (bondState) {
            BluetoothDevice.BOND_BONDED -> {
                Log.i(TAG, "Pairing successful!")
                connectionRetries = 0 // Reset counter
                attemptConnection(device)
            }
            BluetoothDevice.BOND_NONE -> {
                Log.w(TAG, "Pairing failed")
                onConnectionFailed?.invoke("Pairing failed")
            }
        }
    }

    private fun attemptConnection(device: BluetoothDevice) {
        scope.launch {
            try {
                if (device.bondState != BluetoothDevice.BOND_BONDED) {
                    Log.w(TAG, "Device not paired")
                    return@launch
                }

                Log.d(TAG, "Connection attempt (try ${connectionRetries + 1}/$MAX_CONNECTION_RETRIES)")

                val success = connectToRFCOMM(device)

                if (success) {
                    onDeviceConnected?.invoke(device)
                    connectionRetries = 0
                } else {
                    connectionRetries++
                    if (connectionRetries < MAX_CONNECTION_RETRIES) {
                        Log.w(TAG, "Retrying in 5s...")
                        delay(5000)
                        attemptConnection(device)
                    } else {
                        Log.e(TAG, "Connection failed after $MAX_CONNECTION_RETRIES attempts")
                        onConnectionFailed?.invoke("Connection impossible")
                        connectionRetries = 0
                    }
                }

            } catch (e: Exception) {
                Log.e(TAG, "Error in connection attempt", e)
                onConnectionFailed?.invoke("Connection error: ${e.message}")
            }
        }
    }

    private suspend fun connectToRFCOMM(device: BluetoothDevice): Boolean {
        if (!hasBluetoothPermissions()) return false

        var socket: BluetoothSocket? = null

        return withContext(Dispatchers.IO) {
            try {
                Log.d(TAG, "=== RFCOMM ATTEMPT ===")
                Log.d(TAG, "Device: ${device.address}")
                Log.d(TAG, "UUID: $RFCOMM_UUID")

                // Try multiple RFCOMM connection methods
                val methods = listOf(
                    "createRfcommSocketToServiceRecord" to { device.createRfcommSocketToServiceRecord(RFCOMM_UUID) },
                    "createInsecureRfcommSocketToServiceRecord" to { device.createInsecureRfcommSocketToServiceRecord(RFCOMM_UUID) },
                    "createRfcommSocket (channel 1)" to { createRfcommSocketReflection(device, 1) },
                    "createRfcommSocket (channel 3)" to { createRfcommSocketReflection(device, 3) }
                )

                for ((methodName, socketCreator) in methods) {
                    try {
                        Log.d(TAG, "Trying method: $methodName")
                        socket = socketCreator()

                        if (socket == null) {
                            Log.w(TAG, "Null socket with $methodName")
                            continue
                        }

                        Log.d(TAG, "RFCOMM socket created with $methodName")

                        // Connection with timeout
                        val connectJob = async {
                            socket?.connect()
                            Log.i(TAG, "RFCOMM socket.connect() completed with $methodName")
                        }

                        // Wait for connection with timeout
                        val connectSuccess = withTimeoutOrNull(CONNECTION_TIMEOUT_MS) {
                            connectJob.await()
                            true
                        } ?: run {
                            Log.e(TAG, "RFCOMM timeout with $methodName after ${CONNECTION_TIMEOUT_MS}ms")
                            connectJob.cancel()
                            false
                        }

                        if (!connectSuccess) {
                            socket?.close()
                            socket = null
                            continue
                        }

                        // Check connection state
                        if (socket?.isConnected == true) {
                            Log.i(TAG, "✓ RFCOMM connection established with $methodName!")

                            // Communication test
                            val testMessage = "PING"
                            socket?.outputStream?.write(testMessage.toByteArray())
                            socket?.outputStream?.flush()
                            Log.d(TAG, "Test message sent: $testMessage")

                            // Read response
                            val buffer = ByteArray(1024)
                            val bytesRead = withTimeoutOrNull(5000) {
                                socket?.inputStream?.read(buffer) ?: 0
                            } ?: 0

                            if (bytesRead > 0) {
                                val response = String(buffer, 0, bytesRead)
                                Log.d(TAG, "RFCOMM response received: $response")
                            }

                            // Send FCM token to server
                            sendFCMTokenToServer(socket)

                            // Keep connection for test
                            delay(10000)

                            return@withContext true
                        } else {
                            Log.e(TAG, "socket.isConnected = false after connect() with $methodName")
                            socket?.close()
                            socket = null
                        }

                    } catch (e: IOException) {
                        Log.e(TAG, "IOException RFCOMM with $methodName: ${e.message}")
                        socket?.close()
                        socket = null
                        continue

                    } catch (e: SecurityException) {
                        Log.e(TAG, "SecurityException RFCOMM with $methodName", e)
                        socket?.close()
                        socket = null
                        continue

                    } catch (e: Exception) {
                        Log.e(TAG, "Exception RFCOMM with $methodName", e)
                        socket?.close()
                        socket = null
                        continue
                    }
                }

                Log.e(TAG, "All RFCOMM methods failed")
                return@withContext false

            } finally {
                try {
                    socket?.close()
                    Log.d(TAG, "RFCOMM socket closed")
                } catch (e: IOException) {
                    Log.e(TAG, "Error closing RFCOMM socket", e)
                }
            }
        }
    }

    // Reflection method to create RFCOMM socket on specific channel
    private fun createRfcommSocketReflection(device: BluetoothDevice, channel: Int): BluetoothSocket? {
        return try {
            val method = device.javaClass.getMethod("createRfcommSocket", Int::class.javaPrimitiveType)
            method.invoke(device, channel) as BluetoothSocket
        } catch (e: Exception) {
            Log.w(TAG, "RFCOMM reflection failed for channel $channel: ${e.message}")
            null
        }
    }

    private suspend fun sendFCMTokenToServer(socket: BluetoothSocket?) {
        try {
            val token = com.example.test.TokenStorage.getToken(context)
            if (token != null && socket != null) {
                val message = "FCM_TOKEN:$token"
                socket.outputStream.write(message.toByteArray())
                socket.outputStream.flush()
                Log.d(TAG, "FCM token sent to server")

                // Read confirmation with timeout
                withTimeoutOrNull(5000) {
                    val buffer = ByteArray(1024)
                    val bytesRead = socket.inputStream.read(buffer)
                    if (bytesRead > 0) {
                        val response = String(buffer, 0, bytesRead)
                        Log.d(TAG, "Server response: $response")
                    }
                } ?: Log.w(TAG, "Timeout reading server response")

            } else {
                Log.w(TAG, "FCM token not found or socket null")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending FCM token", e)
        }
    }

    // Direct test function for debug with more options
    fun testDirectConnection(useRFCOMM: Boolean = true) {
        scope.launch {
            try {
                val adapter = BluetoothAdapter.getDefaultAdapter()
                val device = adapter.getRemoteDevice("C8:8A:9A:85:44:BC") // Your server

                Log.d(TAG, "=== DIRECT CONNECTION TEST ===")
                Log.d(TAG, "Device: ${device.address}")
                Log.d(TAG, "Name: ${device.name}")
                Log.d(TAG, "Bond state: ${device.bondState}")
                Log.d(TAG, "Type: RFCOMM")
                Log.d(TAG, "Android version: ${android.os.Build.VERSION.SDK_INT}")

                val success = connectToRFCOMM(device)

                if (success) {
                    Log.i(TAG, "✓ DIRECT TEST SUCCESSFUL!")
                } else {
                    Log.e(TAG, "❌ DIRECT TEST FAILED")
                    Log.d(TAG, "Trying RFCOMM alternatives...")
                    testRFCOMMAlternatives(device)
                }

            } catch (e: Exception) {
                Log.e(TAG, "❌ Direct test error: ${e.javaClass.simpleName}: ${e.message}")
                e.printStackTrace()
            }
        }
    }

    private suspend fun testRFCOMMAlternatives(device: BluetoothDevice) {
        // Test with different UUIDs
        val uuids = listOf(
            "00001101-0000-1000-8000-00805F9B34FB" to "Serial Port Profile",
            "00001105-0000-1000-8000-00805F9B34FB" to "OBEX Object Push",
            "0000110A-0000-1000-8000-00805F9B34FB" to "Audio Source",
            "0000110B-0000-1000-8000-00805F9B34FB" to "Audio Sink"
        )

        for ((uuidString, description) in uuids) {
            try {
                Log.d(TAG, "Testing UUID $description: $uuidString")
                val uuid = UUID.fromString(uuidString)
                val socket = device.createRfcommSocketToServiceRecord(uuid)

                val connected = withTimeoutOrNull(10000) {
                    socket.connect()
                    socket.isConnected
                } ?: false

                if (connected) {
                    Log.i(TAG, "✓ Connection successful with $description")
                    socket.outputStream.write("TEST_$description".toByteArray())
                    delay(1000)
                    socket.close()
                    return // Exit function if connection successful
                }
                socket.close()

            } catch (e: Exception) {
                Log.w(TAG, "$description failed: ${e.message}")
            }
        }

        // Test with direct channels
        for (channel in 1..30) {
            try {
                Log.d(TAG, "Testing direct RFCOMM channel: $channel")
                val socket = createRfcommSocketReflection(device, channel)

                if (socket != null) {
                    val connected = withTimeoutOrNull(5000) {
                        socket.connect()
                        socket.isConnected
                    } ?: false

                    if (connected) {
                        Log.i(TAG, "✓ Connection successful on channel $channel")
                        socket.outputStream.write("TEST_CHANNEL_$channel".toByteArray())
                        delay(1000)
                        socket.close()
                        return // Exit function if connection successful
                    }
                    socket.close()
                }

            } catch (e: Exception) {
                Log.v(TAG, "Channel $channel failed: ${e.message}")
            }
        }

        Log.e(TAG, "All RFCOMM alternatives failed")
    }

    private fun hasBluetoothPermissions(): Boolean {
        val permissions = if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.ACCESS_FINE_LOCATION
            )
        } else {
            arrayOf(
                Manifest.permission.BLUETOOTH,
                Manifest.permission.BLUETOOTH_ADMIN,
                Manifest.permission.ACCESS_FINE_LOCATION
            )
        }

        return permissions.all {
            ActivityCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED
        }
    }
}