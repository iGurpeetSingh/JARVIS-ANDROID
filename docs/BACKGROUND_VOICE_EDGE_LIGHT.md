# Background Voice and RGB Edge Lighting

- The native C++ wake-word spotter and offline Hindi/English Whisper recognizer remain the voice engine.
- A microphone foreground service is started while GURI is visible and after RECORD_AUDIO is granted.
- Android overlay access is optional for voice commands. The RGB border is a transparent, touch-through system overlay shown only after the GURI wake word is detected and hidden as soon as a command is executed. Without overlay access, voice commands continue to work and only the border is unavailable.
- WhatsApp, YouTube, Instagram, and Facebook launch commands hide the border before opening the requested app.
- The foreground-service notification provides a Stop listening action. Stopping the service stops background microphone capture.

## First-run setup
1. Open GURI while the app is visible.
2. Allow microphone permission.
3. Optional: allow "Display over other apps" to enable the RGB edge light. Voice commands work without this permission.
4. Keep the persistent GURI voice notification visible while using background voice commands.
5. Say the configured wake word ("GURI" or "HEY GURI"), followed by a command such as "WhatsApp open", "Instagram open", or "Facebook open".

## Platform constraints
- The microphone foreground service must be started while the app is visible and RECORD_AUDIO is already granted on Android 14+.
- RGB drawing requires user-granted SYSTEM_ALERT_WINDOW special access.
- The service uses the existing native AAudio/KWS/Whisper pipeline; it adds no cloud speech provider.
- Another app actively using the microphone can temporarily prevent GURI from receiving audio.
- CI build success does not replace physical-device validation of service lifetime, microphone routing, overlay geometry, and target-app launches.
