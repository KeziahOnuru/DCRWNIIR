package com.example.test

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage
import android.util.Log
import com.example.test.TokenStorage

class FCMService : FirebaseMessagingService() {

    private val TAG = "FCMService"

    override fun onNewToken(token: String) {
        super.onNewToken(token)
        Log.d(TAG, "Token FCM mis à jour: ${token.take(20)}...")
        TokenStorage.saveToken(this, token)
    }

    override fun onMessageReceived(remoteMessage: RemoteMessage) {
        Log.d(TAG, "Message FCM reçu de: ${remoteMessage.from}")

        // Extraire le message
        val message = remoteMessage.data["message"]
            ?: remoteMessage.notification?.body
            ?: "Nouveau message"

        val title = remoteMessage.data["title"]
            ?: remoteMessage.notification?.title
            ?: "Notification"

        // Afficher la notification
        showNotification(title, message)

        // Logger les données pour debug
        if (remoteMessage.data.isNotEmpty()) {
            Log.d(TAG, "Données FCM: ${remoteMessage.data}")
        }
    }

    private fun showNotification(title: String, message: String) {
        val channelId = "fcm_messages"

        // Créer le canal de notification (Android 8+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                channelId,
                "Messages FCM",
                NotificationManager.IMPORTANCE_DEFAULT
            ).apply {
                description = "Notifications des messages Firebase"
                enableLights(true)
                enableVibration(true)
            }

            val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            manager.createNotificationChannel(channel)
        }

        // Créer la notification
        val notification = NotificationCompat.Builder(this, channelId)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentTitle(title)
            .setContentText(message)
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)
            .setAutoCancel(true)
            .setStyle(NotificationCompat.BigTextStyle().bigText(message))
            .build()

        try {
            NotificationManagerCompat.from(this).notify(
                System.currentTimeMillis().toInt(), // ID unique
                notification
            )
            Log.d(TAG, "Notification affichée")
        } catch (e: SecurityException) {
            Log.e(TAG, "Permission notification manquante", e)
        }
    }
}