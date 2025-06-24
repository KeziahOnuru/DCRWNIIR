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
        Log.d(TAG, "FCM token updated: ${token.take(20)}...")
        TokenStorage.saveToken(this, token)
    }

    override fun onMessageReceived(remoteMessage: RemoteMessage) {
        Log.d(TAG, "FCM message received from: ${remoteMessage.from}")

        // Extract the message content
        val message = remoteMessage.data["message"]
            ?: remoteMessage.notification?.body
            ?: "New message"

        val title = remoteMessage.data["title"]
            ?: remoteMessage.notification?.title
            ?: "Notification"

        // Show the notification
        showNotification(title, message)

        // Log message data for debugging
        if (remoteMessage.data.isNotEmpty()) {
            Log.d(TAG, "FCM data: ${remoteMessage.data}")
        }
    }

    private fun showNotification(title: String, message: String) {
        val channelId = "fcm_messages"

        // Create notification channel (Android 8+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                channelId,
                "FCM Messages",
                NotificationManager.IMPORTANCE_DEFAULT
            ).apply {
                description = "Firebase message notifications"
                enableLights(true)
                enableVibration(true)
            }

            val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            manager.createNotificationChannel(channel)
        }

        // Build the notification
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
                System.currentTimeMillis().toInt(), // Unique ID
                notification
            )
            Log.d(TAG, "Notification displayed")
        } catch (e: SecurityException) {
            Log.e(TAG, "Missing notification permission", e)
        }
    }
}
