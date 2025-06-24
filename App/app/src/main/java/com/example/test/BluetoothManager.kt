package com.example.test.bluetooth

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

class BluetoothManager(private val context: Context) {

    private val TAG = "BluetoothManager"
    private val TARGET_NAME = "DCRWNIIR"
    private val PSM = 0x1003

    // Optimized configuration for battery saving
    private val SCAN_INTERVAL_MS = 60_000L // 1 minute instead of 5 seconds
    private val SCAN_DURATION_MS = 12_000L // 12 seconds active scanning
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
                    Log.d(TAG, "Scan finished")
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

        scanJob?.cancel()

        scanJob = scope.launch {
            while (isActive) {
                try {
                    Log.d(TAG, "Starting optimized scan cycle")
                    startActiveScanning()

                    delay(SCAN_DURATION_MS)

                    if (isScanning) {
                        stopActiveScanning()
                    }

                    Log.d(TAG, "Waiting ${SCAN_INTERVAL_MS / 1000}s before next scan")
                    delay(SCAN_INTERVAL_MS)

                } catch (e: CancellationException) {
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Error during scan cycle", e)
                    delay(30_000)
                }
            }
        }
    }

    private fun startActiveScanning() {
        bluetoothAdapter?.let { adapter ->
            if (!adapter.isEnabled) {
                Log.w(TAG, "Bluetooth is off")
                return
            }

            if (isScanning) {
                adapter.cancelDiscovery()
            }

            Log.d(TAG, "Starting active scan...")
            isScanning = adapter.startDiscovery()
            Log.d(TAG, "startDiscovery() launched? $isScanning")

            if (!isScanning) {
                Log.e(TAG, "Failed to start scanning")
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
        Log.d(TAG, "Next scan scheduled in ${SCAN_INTERVAL_MS / 1000}s")
    }

    private fun handleDeviceFound(device: BluetoothDevice) {
        if (!hasBluetoothPermissions()) return

        val deviceName = try { device.name } catch (e: SecurityException) { "Unknown" }

        if (deviceName?.contains(TARGET_NAME, ignoreCase = true) == true) {
            Log.i(TAG, "Target device found: $deviceName - ${device.address}")
            stopActiveScanning()

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
        }
    }

    private fun startPairing(device: BluetoothDevice) {
        if (!hasBluetoothPermissions()) return

        scope.launch {
            try {
                Log.d(TAG, "Trying to pair with ${device.address}")
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
                connectionRetries = 0
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

                Log.d(TAG, "Attempting L2CAP connection (try ${connectionRetries + 1}/$MAX_CONNECTION_RETRIES)")
                val success = connectToL2CAP(device)

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
                        Log.e(TAG, "Failed after $MAX_CONNECTION_RETRIES attempts")
                        onConnectionFailed?.invoke("Unable to connect")
                        connectionRetries = 0
                    }
                }

            } catch (e: Exception) {
                Log.e(TAG, "Error during connection attempt", e)
                onConnectionFailed?.invoke("Connection error: ${e.message}")
            }
        }
    }

    private suspend fun connectToL2CAP(device: BluetoothDevice): Boolean {
        if (!hasBluetoothPermissions()) return false

        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.Q) {
            Log.e(TAG, "L2CAP requires Android 10+")
            return false
        }

        var socket: BluetoothSocket? = null

        return withContext(Dispatchers.IO) {
            try {
                Log.d(TAG, "=== STARTING L2CAP CONNECTION ===")
                Log.d(TAG, "Device: ${device.address}")
                Log.d(TAG, "PSM: 0x${PSM.toString(16)} ($PSM)")
                Log.d(TAG, "Bond state: ${device.bondState}")

                stopActiveScanning()

                socket = device.createL2capChannel(PSM)
                Log.d(TAG, "L2CAP socket created successfully")

                val connectJob = async {
                    socket?.connect()
                    Log.i(TAG, "socket.connect() completed successfully")
                }

                withTimeoutOrNull(CONNECTION_TIMEOUT_MS) {
                    connectJob.await()
                } ?: run {
                    Log.e(TAG, "Connection timeout after ${CONNECTION_TIMEOUT_MS}ms")
                    connectJob.cancel()
                    return@withContext false
                }

                if (socket?.isConnected == true) {
                    Log.i(TAG, "L2CAP connection established successfully!")

                    sendFCMTokenToServer(socket)

                    delay(10000)

                    true
                } else {
                    Log.e(TAG, "socket.isConnected = false after connect()")
                    false
                }

            } catch (e: IOException) {
                Log.e(TAG, "IOException during L2CAP connection", e)
                Log.e(TAG, "Error message: ${e.message}")
                Log.e(TAG, "Cause: ${e.cause}")

                when {
                    e.message?.contains("Connection refused") == true -> {
                        Log.e(TAG, "DIAGNOSIS: Server refused connection - check it’s listening on PSM $PSM")
                    }
                    e.message?.contains("Host is down") == true -> {
                        Log.e(TAG, "DIAGNOSIS: Server unreachable - make sure it is running")
                    }
                    e.message?.contains("Permission denied") == true -> {
                        Log.e(TAG, "DIAGNOSIS: Permission denied - check app permissions")
                    }
                    e.message?.contains("Service discovery failed") == true -> {
                        Log.e(TAG, "DIAGNOSIS: Service not found - is PSM $PSM correct?")
                    }
                    e.message?.contains("read failed") == true -> {
                        Log.e(TAG, "DIAGNOSIS: Socket read failed - server closed the connection")
                    }
                    else -> {
                        Log.e(TAG, "DIAGNOSIS: Unknown error - check server and configuration")
                    }
                }
                false

            } catch (e: SecurityException) {
                Log.e(TAG, "SecurityException - Missing L2CAP permission", e)
                false

            } catch (e: IllegalArgumentException) {
                Log.e(TAG, "IllegalArgumentException - Invalid PSM or device", e)
                Log.e(TAG, "Used PSM: $PSM (0x${PSM.toString(16)})")
                false

            } catch (e: Exception) {
                Log.e(TAG, "Unexpected exception", e)
                false

            } finally {
                Log.d(TAG, "=== END L2CAP CONNECTION ATTEMPT ===")
                try {
                    socket?.close()
                    Log.d(TAG, "Socket closed")
                } catch (e: IOException) {
                    Log.e(TAG, "Error closing socket", e)
                }
            }
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
