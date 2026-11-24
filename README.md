# JARVIS-ANDROID

A personal Android voice-assistant project for Motorola and other Android devices.

## Implemented in the current MVP

- Dark JARVIS-inspired Android dashboard built with Kotlin and Jetpack Compose.
- Tap-to-talk speech recognition using Android SpeechRecognizer (Hindi locale requested; actual language support depends on the installed recognition service).
- Spoken responses using Android TextToSpeech.
- Basic commands for media volume up/down, opening YouTube and WhatsApp, and opening the phone dialer.
- A Settings screen with AUTO / ONLINE / OFFLINE mode preference.
- Network validation logic that can choose an online or offline route in AUTO mode.
- GitHub Actions workflow that builds and uploads a debug APK artifact.

## Important limitations — not yet implemented

- No always-on Jarvis wake-word listener. The current microphone flow starts only after the user taps the microphone.
- No foreground microphone service or lock-screen wake-word support.
- No online AI provider is connected. An API key/provider integration is not included.
- No offline LLM runtime or downloadable model is bundled. The OFFLINE setting is currently a saved preference, not a working local AI model.
- AUTO mode currently has network-routing logic only; it does not yet call an online provider or local model.
- WhatsApp automation currently opens WhatsApp only. It does not search contacts or send messages.
- YouTube command currently opens YouTube; voice-driven search is not yet implemented.
- No Motorola physical-device QA has been completed.

## Build

GitHub Actions runs gradle --no-daemon :app:assembleDebug and uploads app/build/outputs/apk/debug/app-debug.apk as an artifact.

Open the Actions tab, select the latest successful Android CI run, and download the jarvis-android-debug-apk artifact. The artifact is a debug build for testing, not a signed production release.

## Roadmap

1. Complete build and resolve compile/runtime issues.
2. Validate microphone, Hindi speech recognition and TTS on a Motorola device.
3. Add a supported on-device keyword-spotting model and compliant foreground-service flow.
4. Integrate an offline speech/LLM runtime and downloadable model management.
5. Add an online AI provider adapter with user-supplied key and no paid usage without consent.
6. Implement safe, permission-aware WhatsApp and YouTube workflows.
7. Add tests, crash handling, privacy disclosures and release signing.
