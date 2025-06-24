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

    // Configuration optimisée pour économiser la batterie
    private val SCAN_INTERVAL_MS = 60_000L // 1 minute au lieu de 5 secondes
    private val SCAN_DURATION_MS = 12_000L // 12 secondes de scan actif
    private val MAX_CONNECTION_RETRIES = 3
    private val CONNECTION_TIMEOUT_MS = 30_000L // 30 secondes timeout

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
                    Log.d(TAG, "Scan terminé")
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
            Log.e(TAG, "Erreur registration receiver", e)
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
            Log.e(TAG, "Erreur unregister receiver", e)
        }
    }

    private fun startOptimizedScanning() {
        if (!hasBluetoothPermissions()) {
            Log.e(TAG, "Permissions Bluetooth manquantes")
            return
        }

        // Annuler le scan précédent
        scanJob?.cancel()

        // Nouveau cycle de scan optimisé
        scanJob = scope.launch {
            while (isActive) {
                try {
                    Log.d(TAG, "Démarrage cycle de scan optimisé")
                    startActiveScanning()

                    // Attendre la fin du scan ou timeout
                    delay(SCAN_DURATION_MS)

                    // Arrêter le scan actif si toujours en cours
                    if (isScanning) {
                        stopActiveScanning()
                    }

                    // Attendre avant le prochain cycle
                    Log.d(TAG, "Attente ${SCAN_INTERVAL_MS / 1000}s avant prochain scan")
                    delay(SCAN_INTERVAL_MS)

                } catch (e: CancellationException) {
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Erreur durant le cycle de scan", e)
                    delay(30_000) // Attendre 30s en cas d'erreur
                }
            }
        }
    }

    private fun startActiveScanning() {
        bluetoothAdapter?.let { adapter ->
            if (!adapter.isEnabled) {
                Log.w(TAG, "Bluetooth désactivé")
                return
            }

            if (isScanning) {
                adapter.cancelDiscovery()
            }

            Log.d(TAG, "Démarrage scan actif...")
            isScanning = adapter.startDiscovery()
            Log.d(TAG, "startDiscovery() lancé ? $isScanning")

            if (!isScanning) {
                Log.e(TAG, "Impossible de démarrer le scan")
            }
        }
    }

    private fun stopActiveScanning() {
        bluetoothAdapter?.let { adapter ->
            if (isScanning && hasBluetoothPermissions()) {
                adapter.cancelDiscovery()
                isScanning = false
                Log.d(TAG, "Scan actif arrêté")
            }
        }
    }

    private fun stopAllScanning() {
        scanJob?.cancel()
        stopActiveScanning()
    }

    private fun scheduleNextScan() {
        // Le prochain scan est déjà programmé dans la boucle principale
        Log.d(TAG, "Prochain scan programmé dans ${SCAN_INTERVAL_MS / 1000}s")
    }

    private fun handleDeviceFound(device: BluetoothDevice) {
        if (!hasBluetoothPermissions()) return

        val deviceName = try { device.name } catch (e: SecurityException) { "Unknown" }

        // Log plus discret pour éviter le spam
        if (deviceName?.contains(TARGET_NAME, ignoreCase = true) == true) {
            Log.i(TAG, "Serveur cible trouvé: $deviceName - ${device.address}")
            stopActiveScanning() // Arrêter immédiatement pour économiser

            when (device.bondState) {
                BluetoothDevice.BOND_BONDED -> {
                    Log.d(TAG, "Device déjà appairé")
                    attemptConnection(device)
                }
                BluetoothDevice.BOND_NONE -> {
                    Log.d(TAG, "Démarrage appairage")
                    startPairing(device)
                }
                BluetoothDevice.BOND_BONDING -> {
                    Log.d(TAG, "Appairage en cours")
                }
            }
        }
    }

    private fun startPairing(device: BluetoothDevice) {
        if (!hasBluetoothPermissions()) return

        scope.launch {
            try {
                Log.d(TAG, "Tentative d'appairage avec ${device.address}")
                val success = device.createBond()
                if (!success) {
                    Log.e(TAG, "Échec demande d'appairage")
                    onConnectionFailed?.invoke("Échec appairage")
                }
            } catch (e: SecurityException) {
                Log.e(TAG, "Permission manquante pour l'appairage", e)
                onConnectionFailed?.invoke("Permission manquante")
            }
        }
    }

    private fun handleBondStateChanged(device: BluetoothDevice, bondState: Int) {
        if (!hasBluetoothPermissions()) return

        val deviceName = try { device.name } catch (e: SecurityException) { "Unknown" }

        if (deviceName?.equals(TARGET_NAME, ignoreCase = true) != true) return

        when (bondState) {
            BluetoothDevice.BOND_BONDED -> {
                Log.i(TAG, "Appairage réussi!")
                connectionRetries = 0 // Reset compteur
                attemptConnection(device)
            }
            BluetoothDevice.BOND_NONE -> {
                Log.w(TAG, "Appairage échoué")
                onConnectionFailed?.invoke("Appairage échoué")
            }
        }
    }

    private fun attemptConnection(device: BluetoothDevice) {
        scope.launch {
            try {
                if (device.bondState != BluetoothDevice.BOND_BONDED) {
                    Log.w(TAG, "Device pas appairé")
                    return@launch
                }

                Log.d(TAG, "Tentative connexion L2CAP (essai ${connectionRetries + 1}/$MAX_CONNECTION_RETRIES)")
                val success = connectToL2CAP(device)

                if (success) {
                    onDeviceConnected?.invoke(device)
                    connectionRetries = 0
                } else {
                    connectionRetries++
                    if (connectionRetries < MAX_CONNECTION_RETRIES) {
                        Log.w(TAG, "Nouvelle tentative dans 5s...")
                        delay(5000)
                        attemptConnection(device)
                    } else {
                        Log.e(TAG, "Connexion échouée après $MAX_CONNECTION_RETRIES tentatives")
                        onConnectionFailed?.invoke("Connexion impossible")
                        connectionRetries = 0
                    }
                }

            } catch (e: Exception) {
                Log.e(TAG, "Erreur tentative connexion", e)
                onConnectionFailed?.invoke("Erreur connexion: ${e.message}")
            }
        }
    }

    private suspend fun connectToL2CAP(device: BluetoothDevice): Boolean {
        if (!hasBluetoothPermissions()) return false

        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.Q) {
            Log.e(TAG, "L2CAP nécessite Android 10+")
            return false
        }

        var socket: BluetoothSocket? = null

        return withContext(Dispatchers.IO) {
            try {
                Log.d(TAG, "=== DÉBUT CONNEXION L2CAP ===")
                Log.d(TAG, "Device: ${device.address}")
                Log.d(TAG, "PSM: 0x${PSM.toString(16)} ($PSM)")
                Log.d(TAG, "Bond state: ${device.bondState}")

                // Arrêter le scan pendant la connexion pour éviter les interférences
                stopActiveScanning()

                // Créer le socket L2CAP
                socket = device.createL2capChannel(PSM)
                Log.d(TAG, "Socket L2CAP créé avec succès")

                // Connexion avec timeout
                val connectJob = async {
                    socket?.connect()
                    Log.i(TAG, "socket.connect() terminé avec succès")
                }

                // Attendre la connexion avec timeout
                withTimeoutOrNull(CONNECTION_TIMEOUT_MS) {
                    connectJob.await()
                } ?: run {
                    Log.e(TAG, "Timeout de connexion après ${CONNECTION_TIMEOUT_MS}ms")
                    connectJob.cancel()
                    return@withContext false
                }

                // Vérifier l'état de la connexion
                if (socket?.isConnected == true) {
                    Log.i(TAG, "Connexion L2CAP établie avec succès!")

                    // Envoyer token FCM au serveur
                    sendFCMTokenToServer(socket)

                    // Maintenir connexion pour test
                    delay(10000)

                    true
                } else {
                    Log.e(TAG, "socket.isConnected = false après connect()")
                    false
                }

            } catch (e: IOException) {
                Log.e(TAG, "IOException lors de la connexion L2CAP", e)
                Log.e(TAG, "Message d'erreur: ${e.message}")
                Log.e(TAG, "Cause: ${e.cause}")

                // Détails spécifiques selon le message d'erreur
                when {
                    e.message?.contains("Connection refused") == true -> {
                        Log.e(TAG, "DIAGNOSTIC: Serveur refuse la connexion - vérifiez qu'il écoute sur PSM $PSM")
                    }
                    e.message?.contains("Host is down") == true -> {
                        Log.e(TAG, "DIAGNOSTIC: Serveur inaccessible - vérifiez qu'il est démarré")
                    }
                    e.message?.contains("Permission denied") == true -> {
                        Log.e(TAG, "DIAGNOSTIC: Permission refusée - vérifiez les permissions")
                    }
                    e.message?.contains("Service discovery failed") == true -> {
                        Log.e(TAG, "DIAGNOSTIC: Service non trouvé - PSM $PSM incorrect ?")
                    }
                    e.message?.contains("read failed") == true -> {
                        Log.e(TAG, "DIAGNOSTIC: Échec lecture socket - connexion fermée côté serveur")
                    }
                    else -> {
                        Log.e(TAG, "DIAGNOSTIC: Erreur inconnue - vérifiez serveur et configuration")
                    }
                }
                false

            } catch (e: SecurityException) {
                Log.e(TAG, "SecurityException - Permission L2CAP manquante", e)
                false

            } catch (e: IllegalArgumentException) {
                Log.e(TAG, "IllegalArgumentException - PSM invalide ou device invalide", e)
                Log.e(TAG, "PSM utilisé: $PSM (0x${PSM.toString(16)})")
                false

            } catch (e: Exception) {
                Log.e(TAG, "Exception inattendue", e)
                false

            } finally {
                Log.d(TAG, "=== FIN TENTATIVE CONNEXION ===")
                try {
                    socket?.close()
                    Log.d(TAG, "Socket fermé")
                } catch (e: IOException) {
                    Log.e(TAG, "Erreur fermeture socket", e)
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
                Log.d(TAG, "Token FCM envoyé au serveur")

                // Lire confirmation avec timeout
                withTimeoutOrNull(5000) {
                    val buffer = ByteArray(1024)
                    val bytesRead = socket.inputStream.read(buffer)
                    if (bytesRead > 0) {
                        val response = String(buffer, 0, bytesRead)
                        Log.d(TAG, "Réponse serveur: $response")
                    }
                } ?: Log.w(TAG, "Timeout lecture réponse serveur")

            } else {
                Log.w(TAG, "Token FCM introuvable ou socket null")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Erreur envoi token FCM", e)
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