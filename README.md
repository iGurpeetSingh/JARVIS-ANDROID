# JARVIS-ANDROID — Native C++ Android Assistant

The Android application code is **native C++17**. Kotlin source files, Java Activity source files, Kotlin Gradle plugins, Kotlin DSL Gradle files, Jetpack Compose and AndroidX Compose dependencies have been removed.

## Native application architecture

- **C++17 + Android NDK + CMake:** application entry point, command recognition, audio capture, ASR inference and system-action orchestration.
- **Android NativeActivity + NDK native_app_glue:** lifecycle and input integration without an app-authored Kotlin/Java Activity.
- **OpenGL ES 2.0:** native C++ dashboard rendering.
- **AAudio:** 16 kHz mono PCM microphone capture.
- **Sherpa-ONNX C++ API 2.0.3 + ONNX Runtime:** on-device offline Whisper ASR.
- **Whisper Tiny multilingual int8 model:** offline speech-to-text model packaged in the APK. Hindi and other multilingual recognition is requested using Whisper's automatic language detection.
- **JNI calls from C++:** invoke Android platform APIs for runtime permission, launching apps and media-volume control.
- **Groovy Gradle files:** Android packaging/build configuration only; no Kotlin application source or Kotlin plugin remains.

## Implemented and build-verified

- Native C++ NativeActivity launcher.
- Native OpenGL dashboard and microphone/WhatsApp/YouTube touch controls.
- Native AAudio microphone capture at 16 kHz mono PCM.
- Offline Whisper Tiny multilingual speech-to-text from captured audio.
- Native C++ command classification for volume up/down, opening WhatsApp, opening YouTube and dialer intent recognition.
- Native Android platform calls for opening WhatsApp/YouTube and media volume control.
- Kotlin-free CI source check.
- Debug APK built and uploaded using GitHub Actions.

## How voice input currently works

1. Tap the central microphone control to start recording.
2. Speak a short Hindi, English or mixed-language command.
3. Tap the microphone again to stop recording.
4. The app runs Whisper Tiny locally, without sending microphone audio to a remote speech service.
5. The recognized text is passed to the native C++ command classifier. Recognized supported commands execute their Android action.

The current input is **tap-to-record and tap-to-transcribe**. It is not an always-on wake-word listener yet. The offline model and Sherpa-ONNX runtime are packaged with the arm64-v8a debug APK; first use copies model assets to the app-private files directory.

## Not yet complete

- Always-on "Jarvis" wake-word detection and background foreground-service operation.
- Voice activity detection / automatic end-of-utterance detection; the user currently taps again to stop recording.
- Hindi/English spoken response (TTS) using a bundled native voice.
- Online AI provider connection and user-configured credentials.
- Offline LLM conversation runtime.
- WhatsApp contact lookup by name, resolving similar contacts, opening the chosen chat and user-confirmed message sending. Current feature only launches WhatsApp.
- YouTube voice search.
- Motorola physical-device testing, battery/performance profiling and production signing.

## Build

Run `bash scripts/prepare-native-deps.sh` before the Android build. The script downloads the pinned Sherpa-ONNX Android native libraries and the multilingual Whisper Tiny model, validates the Sherpa runtime archive checksum, and places native libraries/headers/model assets in generated build input directories.

Then run:

`gradle --no-daemon :app:assembleDebug`

The build targets `arm64-v8a` for current Motorola/64-bit Android devices. GitHub Actions runs the dependency preparation, verifies that no Kotlin/Java app sources or Kotlin/Compose Gradle dependencies exist, builds the APK and uploads `jarvis-android-debug-apk`.

The debug APK is a test build, not a signed production release.

## Native C++ next milestones

1. Add open-vocabulary "Jarvis" wake-word detection to the live microphone audio stream.
2. Add VAD and hands-free command capture after the wake word.
3. Add native Hindi/English speech synthesis.
4. Add offline LLM runtime and a model-size/device compatibility profile.
5. Add optional online AI through a user-configured provider.
6. Implement safe, permission-aware WhatsApp contact workflows and test on the Motorola device.
7. Add automated tests, crash recovery, battery profiling, privacy disclosures and release signing.
