package com.jarvis.assistant.core

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities

/**
 * Chooses a preferred AI route based on current network availability.
 * This is a routing decision only; provider/model runtimes are integrated separately.
 */
enum class AiMode { AUTO, ONLINE_ONLY, OFFLINE_ONLY }
enum class AiRoute { ONLINE, OFFLINE }

class AiModeRouter(context: Context) {
    private val connectivityManager =
        context.applicationContext.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

    fun hasInternetNetwork(): Boolean {
        val network = connectivityManager.activeNetwork ?: return false
        val capabilities = connectivityManager.getNetworkCapabilities(network) ?: return false
        return capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) &&
            capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)
    }

    fun chooseRoute(mode: AiMode = AiMode.AUTO): AiRoute = when (mode) {
        AiMode.ONLINE_ONLY -> AiRoute.ONLINE
        AiMode.OFFLINE_ONLY -> AiRoute.OFFLINE
        AiMode.AUTO -> if (hasInternetNetwork()) AiRoute.ONLINE else AiRoute.OFFLINE
    }
}
