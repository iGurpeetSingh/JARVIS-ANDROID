# JARVIS-ANDROID

A native Android assistant project using **C++ for the command-processing core** and **Kotlin for Android framework integration and the Compose interface**.

## Language and architecture

- **C++17 (Android NDK + CMake):** native command classifier compiled as `libjarvis_native.so`.
- **JNI:** Kotlin calls the native C++ command engine through `NativeCommandEngine`.
- **Kotlin:** Android activity lifecycle, microphone permission, Android SpeechRecognizer/TextToSpeech integration, launching apps, dialer and volume controls.
- **Jetpack Compose:** Android dashboard and settings UI.

## Implemented

- Dark JARVIS-inspired dashboard with tap-to-talk microphone control.
- Hindi locale requested from Android SpeechRecognizer; recognition quality and language support depend on the device's speech service.
- Hindi TextToSpeech attempt with English fallback.
- Native C++ command classification for volume up/down, opening WhatsApp, opening YouTube and opening the dialer.
- Kotlin executes the selected Android system action after C++ returns a command ID.
- AUTO / ONLINE / OFFLINE preference screen and basic validated-network detection/routing helper.
- GitHub Actions installs Android NDK and CMake, builds the native library and packages a debug APK.

## Not yet implemented

- Always-on wake word and background listening service.
- Native C++ audio capture, acoustic feature extraction, wake-word model or offline speech recognition.
- Local/offline LLM runtime and model download/management.
- Online AI provider connection. No API key or paid provider is bundled.
- AUTO mode does not yet execute an online provider or local LLM; current routing is only a foundation.
- WhatsApp accessibility automation, contact disambiguation, chat opening and message sending. Currently JARVIS only launches WhatsApp.
- YouTube voice search and physical Motorola device QA.

## Build

GitHub Actions runs `gradle --no-daemon :app:assembleDebug` with Android NDK `27.2.12479018` and CMake `3.22.1`, then uploads `app/build/outputs/apk/debug/app-debug.apk` as the `jarvis-android-debug-apk` artifact.

Open the **Actions** tab, choose the latest successful **Android CI** run, and download the artifact. It is a debug APK, not a signed production release.

## Next engineering milestones

1. Stabilize the JNI/C++ build and test command classification on-device.
2. Implement C++ audio input processing and integrate a real wake-word model.
3. Add offline ASR and an on-device LLM runtime with model-size/device compatibility checks.
4. Add an online provider adapter with user-supplied credentials and explicit consent for any paid usage.
5. Implement safe Android Accessibility workflows for WhatsApp and YouTube with clear user confirmation before sending messages.
6. Add automated tests, crash handling, privacy disclosures and release signing.
