# JARVIS Release Signing and Privacy Checklist

## Release signing

The repository intentionally does not contain a signing keystore or release passwords. The Android release build now requires these environment variables:

- `JARVIS_RELEASE_STORE_FILE`: absolute path to a private keystore file available to the build runner.
- `JARVIS_RELEASE_STORE_PASSWORD`: keystore password.
- `JARVIS_RELEASE_KEY_ALIAS`: signing key alias.
- `JARVIS_RELEASE_KEY_PASSWORD`: signing key password.

Generate and retain the keystore outside the repository. Back it up securely. Never commit the keystore, passwords, API tokens or local secret files.

Example local key generation command (run only in a secure environment):

`keytool -genkeypair -v -keystore jarvis-release.jks -keyalg RSA -keysize 2048 -validity 10000 -alias jarvis-release`

Configure the four variables in the local build environment or the protected GitHub Actions environment. The release task fails closed if the variables are missing or the keystore path does not exist. Debug builds do not require the release key.

## Current privacy/security behavior

- Microphone permission is requested at runtime. Audio recognition uses the locally bundled Whisper/Sherpa model; captured speech is not uploaded by the current ASR pipeline.
- Wake-word detection and Hindi TTS run using local native models.
- Contacts are queried from Android Contacts Provider only after `READ_CONTACTS` permission is granted. Contact records are not uploaded by this code path.
- WhatsApp message text is read back for confirmation and opened as prefilled text. The app does not press Send; the user performs the final send inside WhatsApp.
- The Qwen GGUF model is downloaded from Hugging Face over HTTPS when the user taps Offline AI setup. Its size and SHA-256 are checked before it is marked installed.
- No online AI provider is currently configured. Do not add a remote endpoint or transmit transcript text until a provider, user consent flow, and secure credential handling are approved.
- No analytics or telemetry SDK is configured in the repository.

## Before publishing

- Add a public privacy policy URL and link it from the app's store listing.
- Add a clear in-app explanation before first microphone and contacts access; obtain and record the necessary user consent.
- Confirm the privacy-policy statements against a release binary and all third-party dependency behavior.
- Run Android permission, lifecycle, crash, abuse and dependency-license reviews.
- Build a signed release only in a protected environment and verify the generated APK/AAB signature.
- Test update signing with the same protected keystore. Losing or replacing the key can prevent compatible updates.

## Not verified by repository CI

CI compilation is not a substitute for a privacy/legal review, security penetration test, or Motorola physical-device acceptance. Those remain mandatory release gates.
