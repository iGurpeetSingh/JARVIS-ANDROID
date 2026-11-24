# JARVIS-ANDROID — Native C++ Android App

The application layer is being migrated to a **native C++17 Android application**. Kotlin source, Kotlin Gradle plugin, Jetpack Compose and AndroidX Compose dependencies have been removed from the app.

## Current native architecture

- **C++17 + Android NDK + CMake:** application entry point, interface rendering, command classification and native microphone capture.
- **Android NativeActivity + NDK native_app_glue:** Android lifecycle and input integration without an app-authored Kotlin or Java Activity.
- **OpenGL ES 2.0:** native C++ dashboard rendering.
- **AAudio:** native microphone PCM input capture.
- **JNI calls from C++:** invoke required Android framework services such as microphone permission and launching installed Android apps / media volume controls.
- **Groovy Gradle files:** build configuration only. Gradle is still required to package an Android APK; it is not Kotlin application code.

## Included in this migration

- C++ NativeActivity launcher and OpenGL dashboard.
- C++ microphone input capture using AAudio after runtime permission is granted.
- C++ buttons to launch installed WhatsApp and YouTube apps.
- C++ Android AudioManager bridge for media volume adjustment.
- C++ command classifier source for Hindi/English command phrases.
- CI check that fails if Kotlin/Java app source or Kotlin/Compose Gradle dependencies are reintroduced.

## Not yet complete

- Microphone capture is implemented, but speech-to-text (ASR) is **not yet connected**. The mic button currently starts/stops audio capture; it does not transcribe speech or execute spoken commands yet.
- The C++ command classifier is implemented but still needs to be connected to an ASR transcript pipeline.
- No always-on wake-word model or background listening service yet.
- No offline LLM runtime or model manager yet.
- No online AI provider yet.
- WhatsApp contact lookup, ambiguity handling and message sending are not yet implemented.
- No Motorola physical-device verification has been performed.
- The present UI is a basic native OpenGL dashboard, not a final visual recreation of the earlier Compose dashboard.

## Build

GitHub Actions installs Android NDK `27.2.12479018` and CMake `3.22.1`, verifies the app has no Kotlin/Java sources or Kotlin/Compose Gradle dependencies, builds with:

`gradle --no-daemon :app:assembleDebug`

The workflow uploads `app/build/outputs/apk/debug/app-debug.apk` as `jarvis-android-debug-apk`. This is a debug APK, not a signed production release.

## Next native C++ milestones

1. Verify the NativeActivity APK build and resolve compiler or linker errors.
2. Add native audio buffering, voice activity detection and ASR integration.
3. Connect C++ transcript classification to native app actions.
4. Integrate an offline wake-word engine and offline LLM/runtime.
5. Add optional online AI through a user-configured provider.
6. Implement permission-aware WhatsApp Accessibility automation and user-confirmed message sending.
7. Test on the target Motorola device and prepare a signed release.
