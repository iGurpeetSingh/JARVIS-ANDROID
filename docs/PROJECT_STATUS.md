# JARVIS Android — Engineering Progress Tracker

Last reviewed: 2026-11-24
Repository: https://github.com/iGurpeetSingh/JARVIS-ANDROID

## Overall estimate

**Estimated implementation progress: 55%**

**Estimated remaining: 45%**

This is a conservative engineering estimate, not a measured runtime or device-test score. Source integration, CI compilation and physical-device validation are tracked separately. Code being present does not make a feature device-verified.

## Module status

| Module | Source status | Verification status |
|---|---|---|
| Native C++/NativeActivity architecture | Implemented | Historical ARM64 CI build passed; latest changes pending CI |
| OpenGL ES dashboard | Basic implementation | Physical screen/responsiveness review pending |
| Audio capture | Implemented | Needs Motorola microphone test |
| Wake-word spotting | C++ KWS integration present | False-positive/false-negative and foreground lifecycle tests pending |
| Voice activity detection | RMS/silence gate and command timeout present | Real speech/noise tuning pending |
| Offline ASR | Whisper Tiny multilingual integration present | Hindi/Hinglish accuracy on target phone pending |
| Command router | Volume, app launch, dialer, contact and YouTube intents | Unit tests added; latest test run pending |
| WhatsApp contact lookup | Android Contacts Provider integration present | Real contact data/duplicate names testing pending |
| WhatsApp message flow | Recipient/text confirmation and manual-send deep-link flow added | CI and physical WhatsApp verification pending |
| YouTube search | Query parsing and Android VIEW intent added | Latest CI and installed-app/browser fallback tests pending |
| Hindi TTS | Piper phonemizer + Sherpa VITS code integrated | Latest CI and pronunciation/audio-device tests pending |
| Offline LLM | llama.cpp/Qwen3 model flow integrated | Historical native build passed; recent context/download integrity changes pending CI; inference/device tests pending |
| Model integrity | Model download progress/timeouts and SHA-256 verification added | Latest CI pending; full download/hash verification on device pending |
| Conversation memory | Up to four recent turns in process memory | Runtime context-quality test pending |
| Online AI provider | Not implemented | Provider choice, credential UX and secure storage design required |
| Background/screen-off wake service | Not implemented | NativeActivity-only C++ limitation; needs architecture decision |
| Motorola device QA | Not started/recorded | Requires physical target device |
| Production release | Not complete | Signing, privacy, crash recovery and soak tests pending |

## Current verified baseline

- Latest clearly verified historical CI run: 45375534996
- Verified commit: cc5b4b01c9b3dde01aacd164ff8d0c0c6ecd074a
- That run passed native C++ command tests, Kotlin/Java-source guard, ARM64 Debug APK build and artifact upload.
- Artifact: jarvis-android-debug-apk
- Run: https://github.com/iGurpeetSingh/JARVIS-ANDROID/actions/runs/45375534996
- Changes after that commit are not considered build-verified until their own GitHub Actions run succeeds.

## Recent implementation changes requiring new CI/device verification

1. Corrected llama.cpp prompt-token budget versus batch/context bounds; old conversation turns are trimmed before oversized prompts are rejected.
2. Serialized TTS launches and prevented wake restart while the local LLM is generating.
3. Reordered shutdown so ASR, LLM and model-download workers finish before TTS and model resources are destroyed.
4. Added requested WhatsApp message parsing in English/Hinglish/Hindi.
5. Added verbal yes/no confirmation before opening a WhatsApp chat with requested message text prefilled. The app does not press Send.
6. Separated the persistent outbound message from the transient ASR command buffer so the confirmation turn cannot erase the text.
7. Prevented arbitrary text from being sent to the LLM while a WhatsApp message confirmation is pending.
8. Added SHA-256 verification for the downloaded Qwen3 GGUF file.
9. Hardened contact cursor/index handling and international phone-number prefix normalization.
10. Updated README to describe actual foreground limitations and the current implementations.

## Platform constraints and unresolved decisions

### Background listening

The app is built around Android NativeActivity and C++ NDK. It currently starts the wake-word listener while JARVIS is foregrounded and stops capture when focus is lost. A persistent Android foreground service is a separate Android component; do not claim screen-off/background wake listening is complete under this architecture. Do not introduce Kotlin/Java application source without explicit approval.

### Online AI

No provider or API credential has been supplied. Do not hardcode a provider key or save secrets in plaintext. Online AI remains optional and must wait for an approved provider and secure credential-entry/storage design.

### Physical device verification

CI validates compilation and packaging, not microphone routing, model performance, WhatsApp intent behavior, speech quality, RAM use, heat or battery life. Those require the Motorola device.

## Definition of complete

JARVIS reaches 100% only after the agreed foreground voice assistant features pass current CI, all command flows and offline inference run on the target Motorola device, privacy/security checks are complete, and the production release build is signed. Any excluded optional feature (online AI/background service) must be explicitly marked out of scope by the project owner rather than silently counted as complete.

## Next execution order

1. Get a green CI run for the current main branch after all recent changes.
2. Fix all compiler/test failures before extending features.
3. Install latest APK on Motorola and validate mic permission, wake word, VAD, ASR and Hindi TTS.
4. Validate contacts, ambiguous-match handling, WhatsApp message confirmation/manual send, and YouTube search.
5. Download Qwen GGUF, verify SHA-256, load it, test offline answers and profile RAM/thermal performance.
6. Resolve online AI scope and background-service architecture with the project owner.
7. Complete crash recovery, security/privacy, battery/soak tests and release signing.
