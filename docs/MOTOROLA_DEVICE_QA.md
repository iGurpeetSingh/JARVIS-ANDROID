# GURI Motorola Device Acceptance Test Checklist

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
| GURI-001 | First launch | Install and open GURI | Native dashboard opens; microphone permission is requested |
| GURI-002 | Microphone permission | Grant RECORD_AUDIO | Foreground wake-word listener starts and status shows listening |
| GURI-003 | Permission denied | Deny microphone permission | No crash; app reports permission requirement and does not capture audio |
| GURI-004 | Wake phrase | Say “GURI” or “Hey GURI” clearly | Wake phrase detected; app enters command capture |
| GURI-005 | False activation | Play normal conversation/audio without wake phrase | No repeated false command activation during the test window |
| GURI-006 | Automatic end-of-speech | After wake phrase say “volume up”, then remain silent | VAD stops capture after the configured silence interval and starts ASR without a second tap |
| GURI-007 | Hindi ASR | Say a supported command in Hindi | Transcript maps to the intended command |
| GURI-008 | Hinglish ASR | Say a supported command in Hinglish | Transcript maps to the intended command |
| GURI-009 | English ASR | Say “open WhatsApp” | WhatsApp opens |
| GURI-010 | Volume | Say “volume badhao” and “volume kam karo” | Media volume changes in the correct direction |
| GURI-011 | Hindi TTS | Trigger several command responses | Audible Hindi response; no clipping, stuck playback or overlapping audio |
| GURI-012 | WhatsApp permission | Request contact search and grant READ_CONTACTS | Permission is requested; after granting, user can repeat the contact request |
| GURI-013 | Exact contact | Search for a contact with a unique full name | Correct contact chat opens |
| GURI-014 | Ambiguous contact | Search for a first name shared by multiple contacts | GURI asks for a more specific/full name; does not guess |
| GURI-015 | Contact not found | Search a nonexistent contact | GURI reports no match; no chat is opened |
| GURI-016 | Message preview confirmation | Say “send message to [name] saying [message]” | GURI reads back the recipient/message and waits for yes/no |
| GURI-017 | Cancel message | Say “no” / “नहीं” at confirmation | Message is cleared and no WhatsApp deep link is opened |
| GURI-018 | Confirm message | Say “yes” / “हाँ” at confirmation | Correct chat opens with message text prefilled; app does not press Send |
| GURI-019 | Manual send | Inspect the WhatsApp composer after confirmation | User sees the intended text and must manually tap Send |
| GURI-020 | YouTube open | Say “YouTube kholo” | YouTube app opens if installed |
| GURI-021 | YouTube search | Say “search YouTube for Punjabi songs” | YouTube search results open with the requested query |
| GURI-022 | Low storage | Attempt offline model setup with less than required free storage | GURI reports insufficient storage and does not start the download |
| GURI-023 | Model download | Tap Offline AI setup on Wi-Fi with sufficient storage | Progress is shown; download finishes; SHA-256 passes before model is marked installed |
| GURI-024 | Model integrity | Inspect installed model marker and SHA | Marker contains the pinned digest; missing/invalid marker does not count as installed |
| GURI-025 | Low RAM | Attempt local inference with very low available RAM | GURI refuses to load and gives a clear low-memory response instead of crashing |
| GURI-026 | Offline conversation | Download model, disable network and ask a general question | Qwen responds locally and Hindi TTS speaks the response |
| GURI-027 | Conversation context | Ask a follow-up referring to the previous question | Recent conversation turns are used appropriately |
| GURI-028 | Background transition | Start the voice listener, press Home or switch to another app, then say the wake phrase | Foreground microphone service remains active; the RGB border appears above the other app after wake detection and does not steal touch input |
| GURI-029 | Foreground resume | Return to GURI after a background voice command | Dashboard returns without restarting the process; one microphone stream remains active and the listener state is accurate |
| GURI-030 | Stability | Use voice, TTS and model inference repeatedly for at least 20 minutes | No crash, stuck thread, runaway memory growth or unrecoverable audio state |
| GURI-031 | First wake RGB startup race | After granting microphone and overlay permissions, trigger the wake phrase immediately as the voice service starts | The first wake request is retained until the service is ready; the RGB border appears once and is removed after command execution |
| GURI-032 | Overlay permission unavailable | Disable "Display over other apps", open GURI and trigger a wake phrase | Voice listener still starts and commands work; Logcat reports that the RGB border is unavailable without crashing |
| GURI-033 | Mic button stays in GURI | With RECORD_AUDIO already granted, open the dashboard and tap the central microphone button once | GURI stays in the foreground dashboard; listening status updates; no return to Home, previous app or Android Settings |
| GURI-034 | First-use mic permission return | Revoke RECORD_AUDIO in Android app settings, reopen GURI and follow the system microphone permission prompt | After Allow, GURI returns to its own dashboard and starts the listener without requiring an app relaunch |
| GURI-035 | Mic button without overlay permission | Keep "Display over other apps" disabled, open GURI and tap the microphone button | Microphone permission/listening works independently; no automatic Overlay Settings screen is opened |
| GURI-036 | Repeated mic tap stability | With microphone permission granted, tap the central mic control 10 times, waiting for the status after each tap | App remains in GURI; capture starts/stops without crash, duplicate stream or unexpected navigation |

## Evidence to capture

- Test date, tester, exact Motorola model and Android version.
- APK version, Git commit SHA and GitHub Actions run URL.
- Pass/fail for every test ID, with a short failure note.
- Screen recording or screenshot for UI/permission/confirmation cases.
- Logcat output filtered by GURI_NATIVE, GURI_PERMISSION and GURI_ACTIVITY for native input, Android permission and Activity lifecycle diagnostics.
- Available RAM and free storage before Qwen load/download.
- Model SHA-256 verification result.

## Sign-off

Physical-device QA is complete only after all mandatory foreground assistant tests pass or are explicitly waived by the project owner. Screen-off/background wake listening is not part of the current NativeActivity implementation and must remain listed as a separate architecture decision.
