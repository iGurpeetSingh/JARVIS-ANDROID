# JARVIS-ANDROID

A personal Android voice assistant project for Motorola and other Android devices.

## Current milestone: Android MVP

The first milestone focuses on a buildable Android app with:
- Dark JARVIS-inspired dashboard
- Tap-to-talk speech recognition (Hindi/English support depends on the device speech service)
- Spoken responses using Android TextToSpeech
- Basic safe commands: open YouTube, open WhatsApp, open dialer, volume up/down
- Online/offline mode status placeholder (AI providers and local model runtime are not wired yet)

## Important implementation notes

- The current MVP does **not** provide always-on wake-word detection. A reliable "Jarvis" hotword listener requires a dedicated on-device keyword-spotting model and a foreground microphone service, with Android-version and OEM restrictions.
- The current MVP does **not** include a cloud AI API or a downloadable local LLM yet. Those are later milestones.
- Messaging and calling are not silently executed. The app opens the relevant Android app/dialer; sensitive actions require user interaction.
- Online AI requires a provider API key. Never commit API keys to this repository.

## Build

The GitHub Actions workflow builds a debug APK using Gradle installed by the workflow. Download the artifact from the Actions run.

## Roadmap

1. Buildable Android MVP and CI
2. Speech/command tests and Motorola device validation
3. On-device wake-word detection + foreground service
4. Offline speech and local LLM integration
5. Online AI provider adapter and automatic routing
6. Permission-aware phone automation
7. Release APK signing and installation guide
