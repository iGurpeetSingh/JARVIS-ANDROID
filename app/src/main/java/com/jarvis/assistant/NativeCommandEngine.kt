package com.jarvis.assistant

/**
 * Thin JNI bridge. Command classification is implemented in C++.
 * Android platform actions remain in Kotlin because they use Android framework APIs.
 */
class NativeCommandEngine {
    external fun classifyCommand(command: String): Int

    companion object {
        const val UNKNOWN = 0
        const val VOLUME_UP = 1
        const val VOLUME_DOWN = 2
        const val OPEN_WHATSAPP = 3
        const val OPEN_YOUTUBE = 4
        const val OPEN_DIALER = 5

        init {
            System.loadLibrary("jarvis_native")
        }
    }
}
