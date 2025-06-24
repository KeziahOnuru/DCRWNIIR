package com.example.test

import android.app.*
import android.bluetooth.BluetoothDevice
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.util.Log
import androidx.core.app.NotificationCompat
import com.example.test.R
import com.example.test.BluetoothManager
import kotlinx.coroutines.*

class BluetoothAutoConnectService : Service() {

    private val TAG = "BluetoothService"
    private val NOTIFICATION_ID = 1
    private val CHANNEL_ID = "bluetooth_service_channel"

    private lateinit var bluetoothManager: BluetoothManager
    private var wakeLock: PowerManager.WakeLock? = null
    private val serviceScope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "Service created")

        createNotificationChannel()
        startForeground(NOTIFICATION_ID, createNotification("Initializing..."))

        // Acquire partial wake lock to maintain service
        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = powerManager.newWakeLock(
            PowerManager.PARTIAL_WAKE_LOCK,
            "BluetoothService::WakeLock"
        )
        wakeLock?.acquire(60*60*1000L) // 1 hour max

        setupBluetoothManager()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "Service started")

        // Check if it's a test command
        val action = intent?.getStringExtra("action")
        if (action == "test_connection") {
            Log.d(TAG, "Test command received: RFCOMM")
            updateNotification("RFCOMM test in progress...")

            serviceScope.launch {
                bluetoothManager.testDirectConnection(useRFCOMM = true)
                delay(5000)
                updateNotification("Test completed - Normal mode")
            }
            return START_STICKY
        }

        // Launch automatic test after delay (only on first start)
        serviceScope.launch {
            delay(5000) // Wait 5 seconds after startup
            launchDirectTest()
        }

        return START_STICKY // Restart automatically if killed
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "Service destroyed")

        bluetoothManager.cleanup()
        serviceScope.cancel()

        wakeLock?.let {
            if (it.isHeld) {
                it.release()
            }
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun setupBluetoothManager() {
        bluetoothManager = BluetoothManager(this)

        bluetoothManager.onDeviceConnected = { device ->
            Log.i(TAG, "Device connected: ${device.address}")
            updateNotification("Connected to ${device.address}")
        }

        bluetoothManager.onConnectionFailed = { error ->
            Log.w(TAG, "Connection failed: $error")
            updateNotification("Searching... ($error)")
        }

        bluetoothManager.initialize()
        updateNotification("Searching for server...")
    }

    private fun launchDirectTest() {
        Log.d(TAG, "=== LAUNCHING DIRECT TEST ===")
        updateNotification("Direct connection test...")

        serviceScope.launch {
            try {
                // Test RFCOMM
                Log.d(TAG, "🧪 Testing RFCOMM connection...")
                updateNotification("Testing RFCOMM...")
                bluetoothManager.testDirectConnection(useRFCOMM = true)

                // Return to normal mode after test
                delay(10000)
                updateNotification("Test completed - Normal mode")

            } catch (e: Exception) {
                Log.e(TAG, "Error during tests", e)
                updateNotification("Test failed - Normal mode")
            }
        }
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Bluetooth Service",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Bluetooth auto-connect service"
                setShowBadge(false)
            }

            val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            notificationManager.createNotificationChannel(channel)
        }
    }

    private fun createNotification(message: String): Notification {
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Bluetooth Auto-Connect")
            .setContentText(message)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    private fun updateNotification(message: String) {
        val notification = createNotification(message)
        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(NOTIFICATION_ID, notification)
    }
}