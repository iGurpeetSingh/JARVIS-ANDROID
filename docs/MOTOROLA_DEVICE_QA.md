# JARVIS Motorola Device Acceptance Test Checklist

Use this checklist on the actual target Motorola Android phone. Do not mark any case as passed from a CI build alone.

## Preconditions

- Install the latest Debug APK artifact from a successful GitHub Actions run for the current commit.
- Confirm the phone is arm64-v8a and Android API 28 or newer.
- Keep at least 550 MiB free before downloading the Qwen model.
- Ensure WhatsApp and YouTube are installed for app-specific tests.
- Use a quiet room first, then repeat speech tests in normal background noise.
- Record exact phone model, Android version, build commit, test date and tester.

## Test cases

| ID | Test | Steps | Expected result |
|---|---|---|---|
| JARVIS-001 | First launch | Install and open JARVIS | Native dashboard opens; microphone permission is requested |
| JARVIS-002 | Microphone permission | Grant RECORD_AUDIO | Foreground wake-word listener starts and status shows listening |
| JARVIS-003 | Permission denied | Deny microphone permission | No crash; app reports permission requirement and does not capture audio |
| JARVIS-004 | Wake phrase | Say “Hey Jarvis” clearly | Wake phrase detected; app enters command capture |
| JARVIS-005 | False activation | Play normal conversation/audio without wake phrase | No repeated false command activation during the test window |
| JARVIS-006 | Automatic end-of-speech | After wake phrase say “volume up”, then remain silent | VAD stops capture after the configured silence interval and starts ASR without a second tap |
| JARVIS-007 | Hindi ASR | Say a supported command in Hindi | Transcript maps to the intended command |
| JARVIS-008 | Hinglish ASR | Say a supported command in Hinglish | Transcript maps to the intended command |
| JARVIS-009 | English ASR | Say “open WhatsApp” | WhatsApp opens |
| JARVIS-010 | Volume | Say “volume badhao” and “volume kam karo” | Media volume changes in the correct direction |
| JARVIS-011 | Hindi TTS | Trigger several command responses | Audible Hindi response; no clipping, stuck playback or overlapping audio |
| JARVIS-012 | WhatsApp permission | Request contact search and grant READ_CONTACTS | Permission is requested; after granting, user can repeat the contact request |
| JARVIS-013 | Exact contact | Search for a contact with a unique full name | Correct contact chat opens |
| JARVIS-014 | Ambiguous contact | Search for a first name shared by multiple contacts | JARVIS asks for a more specific/full name; does not guess |
| JARVIS-015 | Contact not found | Search a nonexistent contact | JARVIS reports no match; no chat is opened |
| JARVIS-016 | Message preview confirmation | Say “send message to [name] saying [message]” | JARVIS reads back the recipient/message and waits for yes/no |
| JARVIS-017 | Cancel message | Say “no” / “नहीं” at confirmation | Message is cleared and no WhatsApp deep link is opened |
| JARVIS-018 | Confirm message | Say “yes” / “हाँ” at confirmation | Correct chat opens with message text prefilled; app does not press Send |
| JARVIS-019 | Manual send | Inspect the WhatsApp composer after confirmation | User sees the intended text and must manually tap Send |
| JARVIS-020 | YouTube open | Say “YouTube kholo” | YouTube app opens if installed |
| JARVIS-021 | YouTube search | Say “search YouTube for Punjabi songs” | YouTube search results open with the requested query |
| JARVIS-022 | Low storage | Attempt offline model setup with less than required free storage | JARVIS reports insufficient storage and does not start the download |
| JARVIS-023 | Model download | Tap Offline AI setup on Wi-Fi with sufficient storage | Progress is shown; download finishes; SHA-256 passes before model is marked installed |
| JARVIS-024 | Model integrity | Inspect installed model marker and SHA | Marker contains the pinned digest; missing/invalid marker does not count as installed |
| JARVIS-025 | Low RAM | Attempt local inference with very low available RAM | JARVIS refuses to load and gives a clear low-memory response instead of crashing |
| JARVIS-026 | Offline conversation | Download model, disable network and ask a general question | Qwen responds locally and Hindi TTS speaks the response |
| JARVIS-027 | Conversation context | Ask a follow-up referring to the previous question | Recent conversation turns are used appropriately |
| JARVIS-028 | Background transition | Start the voice listener, press Home or switch to another app, then say the wake phrase | Foreground microphone service remains active; the RGB border appears above the other app after wake detection and does not steal touch input |
| JARVIS-029 | Foreground resume | Return to JARVIS after a background voice command | Dashboard returns without restarting the process; one microphone stream remains active and the listener state is accurate |
| JARVIS-030 | Stability | Use voice, TTS and model inference repeatedly for at least 20 minutes | No crash, stuck thread, runaway memory growth or unrecoverable audio state |
| JARVIS-031 | First wake RGB startup race | After granting microphone and overlay permissions, trigger the wake phrase immediately as the voice service starts | The first wake request is retained until the service is ready; the RGB border appears once and is removed after command execution |
| JARVIS-032 | Overlay permission unavailable | Disable "Display over other apps", open JARVIS and trigger a wake phrase | No crash; Logcat reports the missing overlay permission; the app does not claim that the RGB border is visible |

## Evidence to capture

- Test date, tester, exact Motorola model and Android version.
- APK version, Git commit SHA and GitHub Actions run URL.
- Pass/fail for every test ID, with a short failure note.
- Screen recording or screenshot for UI/permission/confirmation cases.
- Logcat output filtered by JARVIS_NATIVE for native errors.
- Available RAM and free storage before Qwen load/download.
- Model SHA-256 verification result.

## Sign-off

Physical-device QA is complete only after all mandatory foreground assistant tests pass or are explicitly waived by the project owner. Screen-off/background wake listening is not part of the current NativeActivity implementation and must remain listed as a separate architecture decision.
