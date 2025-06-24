package com.example.test

import android.content.Context
import android.content.SharedPreferences
import android.util.Log

object TokenStorage {
    private const val TAG = "TokenStorage"
    private const val PREFS_NAME = "FCM_PREFS"
    private const val TOKEN_KEY = "fcm_token"
    private const val TOKEN_TIMESTAMP_KEY = "fcm_token_timestamp"

    // Token is valid for 7 days
    private const val TOKEN_VALIDITY_MS = 7 * 24 * 60 * 60 * 1000L

    fun saveToken(context: Context, token: String) {
        try {
            val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val currentTime = System.currentTimeMillis()

            prefs.edit()
                .putString(TOKEN_KEY, token)
                .putLong(TOKEN_TIMESTAMP_KEY, currentTime)
                .apply()

            Log.d(TAG, "FCM token saved")
        } catch (e: Exception) {
            Log.e(TAG, "Error saving token", e)
        }
    }

    fun getToken(context: Context): String? {
        return try {
            val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val token = prefs.getString(TOKEN_KEY, null)
            val timestamp = prefs.getLong(TOKEN_TIMESTAMP_KEY, 0)

            if (token != null && timestamp > 0) {
                val age = System.currentTimeMillis() - timestamp
                if (age < TOKEN_VALIDITY_MS) {
                    token
                } else {
                    Log.w(TAG, "FCM token expired")
                    clearToken(context)
                    null
                }
            } else {
                null
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error retrieving token", e)
            null
        }
    }

    fun clearToken(context: Context) {
        try {
            val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            prefs.edit()
                .remove(TOKEN_KEY)
                .remove(TOKEN_TIMESTAMP_KEY)
                .apply()
            Log.d(TAG, "FCM token cleared")
        } catch (e: Exception) {
            Log.e(TAG, "Error clearing token", e)
        }
    }

    fun isTokenValid(context: Context): Boolean {
        return getToken(context) != null
    }
}
