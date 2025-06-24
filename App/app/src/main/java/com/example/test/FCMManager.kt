package com.example.test

import android.content.Context
import android.util.Log
import com.google.firebase.messaging.FirebaseMessaging
import kotlinx.coroutines.*

class FCMManager(private val context: Context) {

    private val TAG = "FCMManager"
    private val scope = CoroutineScope(Dispatchers.IO)

    fun initialize(onTokenReady: (String) -> Unit) {
        // Check if we already have a token
        val savedToken = TokenStorage.getToken(context)

        if (savedToken != null) {
            Log.d(TAG, "Existing token found")
            onTokenReady(savedToken)
        } else {
            // Retrieve a new token
            FirebaseMessaging.getInstance().token.addOnCompleteListener { task ->
                if (task.isSuccessful) {
                    val token = task.result
                    if (token != null) {
                        Log.d(TAG, "New FCM token retrieved")
                        TokenStorage.saveToken(context, token)
                        onTokenReady(token)
                    } else {
                        Log.e(TAG, "FCM token is null")
                    }
                } else {
                    Log.e(TAG, "Failed to retrieve FCM token", task.exception)
                }
            }
        }
    }

    fun refreshToken(onTokenRefreshed: (String) -> Unit) {
        FirebaseMessaging.getInstance().deleteToken().addOnCompleteListener { deleteTask ->
            if (deleteTask.isSuccessful) {
                Log.d(TAG, "Old token deleted")

                FirebaseMessaging.getInstance().token.addOnCompleteListener { newTask ->
                    if (newTask.isSuccessful) {
                        val newToken = newTask.result
                        if (newToken != null) {
                            Log.d(TAG, "New token generated")
                            TokenStorage.saveToken(context, newToken)
                            onTokenRefreshed(newToken)
                        }
                    } else {
                        Log.e(TAG, "Failed to generate new token", newTask.exception)
                    }
                }
            } else {
                Log.e(TAG, "Failed to delete old token", deleteTask.exception)
            }
        }
    }

    fun getCurrentToken(): String? {
        return TokenStorage.getToken(context)
    }
}
