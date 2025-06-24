package com.example.test

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.example.test.ui.theme.TestTheme
import com.example.test.BluetoothAutoConnectService
import com.example.test.TokenStorage
import com.example.test.FCMManager
import android.util.Log

class MainActivity : ComponentActivity() {

    private val TAG = "MainActivity"
    private val bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
    private lateinit var fcmManager: FCMManager

    // Required permissions
    private val bluetoothPermissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
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

    private val notificationPermission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        arrayOf(Manifest.permission.POST_NOTIFICATIONS)
    } else {
        emptyArray()
    }

    // Activity result launchers
    private val enableBluetoothLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == RESULT_OK) {
            Log.d(TAG, "Bluetooth enabled")
            startBluetoothService()
        } else {
            Log.w(TAG, "Bluetooth activation denied")
        }
    }

    private val bluetoothPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.all { it.value }
        if (allGranted) {
            Log.d(TAG, "Bluetooth permissions granted")
            checkBluetoothAndStart()
        } else {
            Log.w(TAG, "Bluetooth permissions denied")
        }
    }

    private val notificationPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.all { it.value }
        if (allGranted) {
            Log.d(TAG, "Notification permissions granted")
        } else {
            Log.w(TAG, "Notification permissions denied")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        // Initialize managers
        fcmManager = FCMManager(this)

        // Setup configuration
        setupNotificationPermissions()
        setupFCM()
        setupBluetooth()

        setContent {
            TestTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    AppStatusScreen(
                        modifier = Modifier.padding(innerPadding)
                    )
                }
            }
        }
    }

    private fun setupNotificationPermissions() {
        if (notificationPermission.isNotEmpty()) {
            val missingPermissions = notificationPermission.filter {
                ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
            }

            if (missingPermissions.isNotEmpty()) {
                notificationPermissionLauncher.launch(missingPermissions.toTypedArray())
            }
        }
    }

    private fun setupFCM() {
        fcmManager.initialize { token ->
            Log.d(TAG, "FCM token ready: ${token.take(20)}...")
        }
    }

    private fun setupBluetooth() {
        if (bluetoothAdapter == null) {
            Log.e(TAG, "Bluetooth not supported")
            return
        }

        requestBluetoothPermissions()
    }

    private fun requestBluetoothPermissions() {
        val missingPermissions = bluetoothPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isNotEmpty()) {
            Log.d(TAG, "Requesting Bluetooth permissions")
            bluetoothPermissionLauncher.launch(missingPermissions.toTypedArray())
        } else {
            checkBluetoothAndStart()
        }
    }

    private fun checkBluetoothAndStart() {
        if (bluetoothAdapter?.isEnabled == true) {
            startBluetoothService()
        } else {
            Log.d(TAG, "Requesting Bluetooth activation")
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            enableBluetoothLauncher.launch(enableBtIntent)
        }
    }

    private fun startBluetoothService() {
        Log.d(TAG, "Starting Bluetooth service")
        val serviceIntent = Intent(this, BluetoothAutoConnectService::class.java)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent)
        } else {
            startService(serviceIntent)
        }
    }

    @Composable
    fun AppStatusScreen(modifier: Modifier = Modifier) {
        var bluetoothEnabled by remember { mutableStateOf(bluetoothAdapter?.isEnabled ?: false) }
        var bluetoothPermissions by remember { mutableStateOf(checkBluetoothPermissions()) }
        var notificationPermissions by remember { mutableStateOf(checkNotificationPermissions()) }
        var fcmToken by remember { mutableStateOf(TokenStorage.getToken(this@MainActivity)) }

        // Periodically update statuses
        LaunchedEffect(Unit) {
            while (true) {
                bluetoothEnabled = bluetoothAdapter?.isEnabled ?: false
                bluetoothPermissions = checkBluetoothPermissions()
                notificationPermissions = checkNotificationPermissions()
                fcmToken = TokenStorage.getToken(this@MainActivity)
                kotlinx.coroutines.delay(3000)
            }
        }

        Column(
            modifier = modifier
                .fillMaxSize()
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Text(
                text = "Bluetooth Auto-Connect",
                style = MaterialTheme.typography.headlineMedium
            )

            Spacer(modifier = Modifier.height(16.dp))

            // Bluetooth status
            StatusCard(
                title = "Bluetooth",
                status = if (bluetoothEnabled) "Enabled" else "Disabled",
                isOk = bluetoothEnabled,
                details = "Required for automatic connection"
            )

            // Bluetooth permission status
            StatusCard(
                title = "Bluetooth Permissions",
                status = if (bluetoothPermissions) "Granted" else "Missing",
                isOk = bluetoothPermissions,
                details = "Required for scanning and connecting via Bluetooth + location access"
            )

            // Notification status
            StatusCard(
                title = "Notifications",
                status = if (notificationPermissions) "Enabled" else "Disabled",
                isOk = notificationPermissions,
                details = "Required for FCM messages"
            )

            // FCM token status
            val tokenValue = fcmToken
            StatusCard(
                title = "FCM Token",
                status = if (tokenValue != null) "Available" else "Pending",
                isOk = tokenValue != null,
                details = if (tokenValue != null) {
                    "Token: ${tokenValue.take(20)}..."
                } else {
                    "Token will be sent when connected"
                }
            )

            Spacer(modifier = Modifier.height(16.dp))

            // Action buttons
            val allReady = bluetoothEnabled && bluetoothPermissions && notificationPermissions

            if (!allReady) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    if (!bluetoothEnabled || !bluetoothPermissions) {
                        Button(
                            onClick = { setupBluetooth() }
                        ) {
                            Text("Configure Bluetooth")
                        }
                    }

                    if (!notificationPermissions) {
                        Button(
                            onClick = { setupNotificationPermissions() }
                        ) {
                            Text("Enable Notifications")
                        }
                    }
                }
            } else {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(
                        text = "✓ Fully Configured",
                        style = MaterialTheme.typography.titleMedium,
                        color = MaterialTheme.colorScheme.primary
                    )

                    Text(
                        text = "Service running - scanning every 60s",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                    )

                    Spacer(modifier = Modifier.height(8.dp))

                    Button(
                        onClick = { startBluetoothService() }
                    ) {
                        Text("Restart Service")
                    }
                }
            }
        }
    }

    @Composable
    fun StatusCard(
        title: String,
        status: String,
        isOk: Boolean,
        details: String? = null
    ) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = if (isOk)
                    MaterialTheme.colorScheme.primaryContainer
                else
                    MaterialTheme.colorScheme.errorContainer
            )
        ) {
            Column(
                modifier = Modifier.padding(16.dp)
            ) {
                Row(
                    horizontalArrangement = Arrangement.SpaceBetween,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(
                        text = title,
                        style = MaterialTheme.typography.titleMedium
                    )
                    Text(
                        text = if (isOk) "✓" else "✗",
                        style = MaterialTheme.typography.titleMedium
                    )
                }

                Text(
                    text = status,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.8f)
                )

                details?.let { detail ->
                    Text(
                        text = detail,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
                    )
                }
            }
        }
    }

    private fun checkBluetoothPermissions(): Boolean {
        return bluetoothPermissions.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun checkNotificationPermissions(): Boolean {
        return if (notificationPermission.isNotEmpty()) {
            notificationPermission.all {
                ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
            }
        } else {
            true // No permissions required on older versions
        }
    }
}
