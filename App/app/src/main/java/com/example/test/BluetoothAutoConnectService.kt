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
        Log.d(TAG, "Service créé")

        createNotificationChannel()
        startForeground(NOTIFICATION_ID, createNotification("Initialisation..."))

        // Acquérir un wake lock partiel pour maintenir le service
        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = powerManager.newWakeLock(
            PowerManager.PARTIAL_WAKE_LOCK,
            "BluetoothService::WakeLock"
        )
        wakeLock?.acquire(60*60*1000L) // 1 heure max

        setupBluetoothManager()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "Service démarré")

        // Vérifier si c'est une commande de test
        val action = intent?.getStringExtra("action")
        if (action == "test_connection") {
            val useRFCOMM = intent.getBooleanExtra("use_rfcomm", false)
            Log.d(TAG, "Commande de test reçue: ${if (useRFCOMM) "RFCOMM" else "L2CAP"}")
            updateNotification("Test ${if (useRFCOMM) "RFCOMM" else "L2CAP"} en cours...")

            serviceScope.launch {
                bluetoothManager.testDirectConnection(useRFCOMM)
                delay(5000)
                updateNotification("Test terminé - Mode normal")
            }
            return START_STICKY
        }

        // Lancer le test automatique après un délai (seulement au premier démarrage)
        serviceScope.launch {
            delay(5000) // Attendre 5 secondes après le démarrage
            launchDirectTest()
        }

        return START_STICKY // Redémarrer automatiquement si tué
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "Service détruit")

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
            Log.i(TAG, "Device connecté: ${device.address}")
            updateNotification("Connecté à ${device.address}")
        }

        bluetoothManager.onConnectionFailed = { error ->
            Log.w(TAG, "Connexion échouée: $error")
            updateNotification("Recherche en cours... ($error)")
        }

        bluetoothManager.initialize()
        updateNotification("Recherche du serveur...")
    }

    private fun launchDirectTest() {
        Log.d(TAG, "=== LANCEMENT TEST DIRECT ===")
        updateNotification("Test de connexion directe...")

        serviceScope.launch {
            try {
                // Test L2CAP d'abord
                Log.d(TAG, "🧪 Test connexion L2CAP...")
                updateNotification("Test L2CAP...")
                bluetoothManager.testDirectConnection(useRFCOMM = false)

                // Attendre un peu puis tester RFCOMM
                delay(10000)
                Log.d(TAG, "🧪 Test connexion RFCOMM...")
                updateNotification("Test RFCOMM...")
                bluetoothManager.testDirectConnection(useRFCOMM = true)

                // Retour au mode normal après les tests
                delay(10000)
                updateNotification("Tests terminés - Mode normal")

            } catch (e: Exception) {
                Log.e(TAG, "Erreur lors des tests", e)
                updateNotification("Tests échoués - Mode normal")
            }
        }
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Service Bluetooth",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Service de connexion automatique Bluetooth"
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
