# G U R I — Native C++ Android AI Assistant

G U R I is a voice-first Android assistant built with native C++17, Android NDK, NativeActivity and CMake. The application logic, dashboard, voice pipeline, command routing and AI orchestration remain in C++.

## Non-negotiable architecture

- Native C++17 application logic.
- Android NativeActivity and NDK native_app_glue.
- OpenGL ES dashboard renderer.
- AAudio microphone capture and playback.
- JNI calls from C++ for Android platform APIs.
- Sherpa-ONNX C++ API and ONNX Runtime for offline speech recognition, keyword spotting and speech generation.
- Piper phonemizer compiled for Android using CMake/NDK.
- llama.cpp native runtime for local GGUF inference.
- Groovy Gradle and AndroidManifest are used only for Android packaging and platform configuration.
- No Kotlin application source, Java application source, Kotlin Gradle plugin or Compose dependency.

## Implemented code paths

### Voice pipeline

- Foreground focus requests microphone permission and starts the native wake-word listener once permission is granted.
- Streaming keyword spotting is configured for GURI and HEY GURI.
- After wake detection, a VAD-style RMS/silence gate captures the spoken command.
- Capture stops after approximately 0.9 seconds of silence after speech, or a 10-second command safety limit.
- Whisper Tiny multilingual runs locally for Hindi, English and Hinglish speech-to-text.
- The wake listener pauses when the app loses foreground focus and can restart when focus returns.

### Native commands

- Volume up/down.
- Open WhatsApp.
- Search contacts through Android Contacts Provider.
- Exact and partial name matching, with duplicate matches requiring the user to say a more specific name.
- Open a WhatsApp chat without sending anything.
- Prepare a requested WhatsApp message only after GURI reads back the contact and text and receives a verbal yes/confirmation.
- The final Send action remains manual inside WhatsApp; GURI does not silently send messages.
- Open YouTube and search YouTube using parsed Hindi, Hinglish or English queries.
- Open the phone dialer without placing a call.

### Hindi voice output

- Sherpa-ONNX VITS/Piper Hindi voice model.
- Piper phonemization with bundled eSpeak NG data.
- Generated speech is played through AAudio.
- Voice output is integrated in code but pronunciation, interruptions and reliability still require Motorola device testing.

### Offline AI

- llama.cpp is included as a native C++ dependency.
- Qwen3 0.6B Q4_K_M GGUF can be downloaded from the app using the Offline AI setup area.
- The model is not bundled in the APK; first download requires internet and sufficient storage.
- The download reports progress, uses connection/read timeouts and verifies the final file against a pinned SHA-256 digest before enabling inference.
- Local inference keeps up to four recent conversation turns in process memory.
- Prompt length is bounded to fit the model context and old turns are dropped before rejecting an oversized prompt.
- The model can answer general questions offline after a successful download and load.
- Model load time, RAM use and inference speed depend on the target Motorola model and available memory.

## Current platform limitation

The app uses NativeActivity for its native C++ audio/AI engine and a small Java foreground-service bridge for Android microphone-service compliance and RGB edge overlays. The foreground service must be started while the app is visible and after microphone permission is granted. Overlay permission is optional for voice commands; it is required only for the RGB edge light. Removing the app task from Recents stops the service because the native engine is owned by the Activity. This is not a device-reboot auto-start service.

## Build requirements

1. Run scripts/prepare-native-deps.sh.
2. Build with Gradle using :app:assembleDebug.
3. The current ABI filter is arm64-v8a.
4. The minimum Android API is 28; compile SDK is 35; NDK is 27.2.12479018.

GitHub Actions runs native command-engine regression tests, prepares Sherpa/model assets, checks the absence of Kotlin/Java app sources and Kotlin/Compose Gradle dependencies, and builds/uploads the debug APK.

## Verification policy

A successful CI build proves compilation and packaging for the code revision that triggered that run. It does not prove runtime behavior on a Motorola device. Features count as device-verified only after physical-device testing and recorded results.

## Remaining work

- Keep CI green for every subsequent code change.
- Install and test the latest APK on the target Motorola device.
- Validate wake-word accuracy, false activations, VAD timing and command capture in real environments.
- Validate Hindi ASR quality and TTS pronunciation on-device.
- Test contact matching on actual address books, including duplicate names and international phone formats.
- Test WhatsApp confirmation, prefilled text and manual-send behavior.
- Test YouTube search intents with and without YouTube installed.
- Download the full Qwen model, verify SHA-256, measure RAM/thermal behavior and test local inference.
- Add optional online AI only after a provider, credential-entry UX and secure secret-storage design are approved.
- Decide whether background service behavior can be supported without violating the strict C++ application-source requirement.
- Complete lifecycle, privacy, crash-recovery, long-duration and production-signing checks.

## Latest fully verified CI

Run 45375534996 successfully built and uploaded a debug APK for commit cc5b4b01c9b3dde01aacd164ff8d0c0c6ecd074a. Newer changes must be verified by their own CI run before being called build-verified.

Repository: https://github.com/iGurpeetSingh/JARVIS-ANDROID
Actions: https://github.com/iGurpeetSingh/JARVIS-ANDROID/actions

Engineering progress: docs/PROJECT_STATUS.md
Motorola acceptance tests: docs/MOTOROLA_DEVICE_QA.md
