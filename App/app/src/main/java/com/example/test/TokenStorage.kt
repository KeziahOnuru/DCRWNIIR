package com.example.test

import android.content.Context
import android.content.SharedPreferences

object TokenStorage {
    private const val PREFS_NAME = "FCM_PREFS"
    private const val TOKEN_KEY = "fcm_token"

    fun saveToken(context: Context, token: String) {
        val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putString(TOKEN_KEY, token).apply()
    }

    fun getToken(context: Context): String? {
        val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getString(TOKEN_KEY, null)
    }
}