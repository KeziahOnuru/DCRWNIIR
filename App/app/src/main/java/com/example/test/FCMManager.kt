package com.example.test

import android.content.Context
import android.util.Log
import com.google.firebase.messaging.FirebaseMessaging
import kotlinx.coroutines.*

class FCMManager(private val context: Context) {

    private val TAG = "FCMManager"
    private val scope = CoroutineScope(Dispatchers.IO)

    fun initialize(onTokenReady: (String) -> Unit) {
        // Vérifier si on a déjà un token
        val savedToken = TokenStorage.getToken(context)

        if (savedToken != null) {
            Log.d(TAG, "Token existant trouvé")
            onTokenReady(savedToken)
        } else {
            // Récupérer un nouveau token
            FirebaseMessaging.getInstance().token.addOnCompleteListener { task ->
                if (task.isSuccessful) {
                    val token = task.result
                    if (token != null) {
                        Log.d(TAG, "Nouveau token FCM récupéré")
                        TokenStorage.saveToken(context, token)
                        onTokenReady(token)
                    } else {
                        Log.e(TAG, "Token FCM null")
                    }
                } else {
                    Log.e(TAG, "Échec récupération token FCM", task.exception)
                }
            }
        }
    }

    fun refreshToken(onTokenRefreshed: (String) -> Unit) {
        FirebaseMessaging.getInstance().deleteToken().addOnCompleteListener { deleteTask ->
            if (deleteTask.isSuccessful) {
                Log.d(TAG, "Ancien token supprimé")

                FirebaseMessaging.getInstance().token.addOnCompleteListener { newTask ->
                    if (newTask.isSuccessful) {
                        val newToken = newTask.result
                        if (newToken != null) {
                            Log.d(TAG, "Nouveau token généré")
                            TokenStorage.saveToken(context, newToken)
                            onTokenRefreshed(newToken)
                        }
                    } else {
                        Log.e(TAG, "Échec génération nouveau token", newTask.exception)
                    }
                }
            } else {
                Log.e(TAG, "Échec suppression ancien token", deleteTask.exception)
            }
        }
    }

    fun getCurrentToken(): String? {
        return TokenStorage.getToken(context)
    }
}