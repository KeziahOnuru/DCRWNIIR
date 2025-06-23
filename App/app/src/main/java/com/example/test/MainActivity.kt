package com.example.test

import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.example.test.ui.theme.TestTheme

import android.util.Log
import com.google.firebase.messaging.FirebaseMessaging
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        // Demande de permission pour les notifications
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(arrayOf(android.Manifest.permission.POST_NOTIFICATIONS), 1001)
            }
        }

        val savedToken = TokenStorage.getToken(this)

        if (savedToken == null) {
            FirebaseMessaging.getInstance().token.addOnCompleteListener { task ->
                val token = task.result
                if (token != null) {
                    Log.d("FCM", "Token (new): $token")
                    TokenStorage.saveToken(this, token)
                } else {
                    Log.e("FCM", "Échec de récupération du token")
                }
            }
        } else {
            Log.d("FCM", "Token (cached): $savedToken")
        }
    }
}
