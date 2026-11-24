package com.jarvis.assistant

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioManager
import android.os.Bundle
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import android.speech.tts.TextToSpeech
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.VolumeDown
import androidx.compose.material.icons.filled.VolumeUp
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import java.util.Locale

class MainActivity : ComponentActivity(), TextToSpeech.OnInitListener {
    private var speechRecognizer: SpeechRecognizer? = null
    private var tts: TextToSpeech? = null
    private var listening by mutableStateOf(false)
    private var heardText by mutableStateOf("Tap the microphone and say a command.")
    private var statusText by mutableStateOf("Ready • Tap to speak")
    private var lastResponse by mutableStateOf("Hello. I am JARVIS.")
    private var ttsReady = false

    private val microphonePermission = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { granted ->
        if (granted) startListening() else {
            statusText = "Microphone permission is required for voice input."
            speak("माइक्रोफोन की अनुमति जरूरी है।")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        tts = TextToSpeech(this, this)
        setContent {
            JarvisScreen(
                listening = listening,
                heardText = heardText,
                statusText = statusText,
                response = lastResponse,
                onMicClick = ::onMicClick,
                onVolumeUp = { adjustVolume(true) },
                onVolumeDown = { adjustVolume(false) }
            )
        }
    }

    override fun onInit(status: Int) {
        ttsReady = status == TextToSpeech.SUCCESS
        if (ttsReady) {
            val hindi = tts?.setLanguage(Locale("hi", "IN")) ?: TextToSpeech.LANG_MISSING_DATA
            if (hindi < TextToSpeech.LANG_AVAILABLE) {
                tts?.language = Locale.US
            }
        }
    }

    private fun onMicClick() {
        if (listening) {
            speechRecognizer?.cancel()
            listening = false
            statusText = "Listening cancelled"
            return
        }
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            == PackageManager.PERMISSION_GRANTED
        ) startListening() else microphonePermission.launch(Manifest.permission.RECORD_AUDIO)
    }

    private fun startListening() {
        if (!SpeechRecognizer.isRecognitionAvailable(this)) {
            statusText = "Speech recognition service is not available on this device."
            speak("इस फोन में Speech Recognition सेवा उपलब्ध नहीं है।")
            return
        }
        speechRecognizer?.destroy()
        speechRecognizer = SpeechRecognizer.createSpeechRecognizer(this).also { recognizer ->
            recognizer.setRecognitionListener(object : RecognitionListener {
                override fun onReadyForSpeech(params: Bundle?) {
                    listening = true
                    statusText = "Listening…"
                }
                override fun onBeginningOfSpeech() { statusText = "Hearing your voice…" }
                override fun onRmsChanged(rmsdB: Float) = Unit
                override fun onBufferReceived(buffer: ByteArray?) = Unit
                override fun onEndOfSpeech() { listening = false; statusText = "Processing…" }
                override fun onError(error: Int) {
                    listening = false
                    statusText = "Could not recognize speech. Please try again."
                }
                override fun onResults(results: Bundle?) {
                    listening = false
                    val text = results?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                        ?.firstOrNull().orEmpty()
                    heardText = text.ifBlank { "I did not catch that. Please try again." }
                    handleCommand(text)
                }
                override fun onPartialResults(partialResults: Bundle?) = Unit
                override fun onEvent(eventType: Int, params: Bundle?) = Unit
            })
            val intent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
                putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
                putExtra(RecognizerIntent.EXTRA_LANGUAGE, "hi-IN")
                putExtra(RecognizerIntent.EXTRA_LANGUAGE_PREFERENCE, "hi-IN")
                putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
                putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 3)
            }
            recognizer.startListening(intent)
        }
    }

    private fun handleCommand(raw: String) {
        val command = raw.lowercase(Locale.ROOT)
        when {
            "volume up" in command || "आवाज बढ़" in command || "वॉल्यूम बढ़" in command -> {
                adjustVolume(true)
                respond("वॉल्यूम बढ़ा दिया है।")
            }
            "volume down" in command || "आवाज कम" in command || "वॉल्यूम कम" in command -> {
                adjustVolume(false)
                respond("वॉल्यूम कम कर दिया है।")
            }
            "youtube" in command || "यूट्यूब" in command -> {
                openApp("com.google.android.youtube", "https://www.youtube.com", "YouTube")
            }
            "whatsapp" in command || "व्हाट्सएप" in command -> {
                openApp("com.whatsapp", "https://www.whatsapp.com", "WhatsApp")
            }
            "call" in command || "कॉल" in command || "फोन लगाओ" in command -> {
                runCatching {
                    startActivity(Intent(Intent.ACTION_DIAL))
                    respond("डायलर खोल दिया है। कॉल करने से पहले नंबर जाँच लें।")
                }.onFailure { respond("डायलर नहीं खुल पाया।") }
            }
            else -> respond("मैंने आपका आदेश सुना है, लेकिन यह सुविधा अभी इस MVP में उपलब्ध नहीं है।")
        }
    }

    private fun openApp(packageName: String, fallbackUrl: String, appName: String) {
        val launchIntent = packageManager.getLaunchIntentForPackage(packageName)
        runCatching {
            if (launchIntent != null) startActivity(launchIntent)
            else startActivity(Intent(Intent.ACTION_VIEW, android.net.Uri.parse(fallbackUrl)))
            respond("$appName खोल रहा हूँ।")
        }.onFailure { respond("$appName नहीं खुल पाया।") }
    }

    private fun adjustVolume(up: Boolean) {
        val audio = getSystemService(AUDIO_SERVICE) as AudioManager
        audio.adjustStreamVolume(
            AudioManager.STREAM_MUSIC,
            if (up) AudioManager.ADJUST_RAISE else AudioManager.ADJUST_LOWER,
            AudioManager.FLAG_SHOW_UI
        )
        statusText = if (up) "Media volume increased" else "Media volume decreased"
    }

    private fun respond(message: String) {
        lastResponse = message
        statusText = "Command processed"
        speak(message)
    }

    private fun speak(message: String) {
        if (ttsReady) tts?.speak(message, TextToSpeech.QUEUE_FLUSH, null, "jarvis-response")
    }

    override fun onDestroy() {
        speechRecognizer?.destroy()
        tts?.stop()
        tts?.shutdown()
        super.onDestroy()
    }
}

@Composable
private fun JarvisScreen(
    listening: Boolean,
    heardText: String,
    statusText: String,
    response: String,
    onMicClick: () -> Unit,
    onVolumeUp: () -> Unit,
    onVolumeDown: () -> Unit
) {
    val background = Brush.verticalGradient(
        listOf(Color(0xFF050B16), Color(0xFF0A1B30), Color(0xFF030711))
    )
    MaterialTheme {
        Surface(modifier = Modifier.fillMaxSize(), color = Color(0xFF050B16)) {
            Column(
                modifier = Modifier.fillMaxSize().background(background).padding(20.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(18.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("JARVIS", color = Color.White, fontSize = 26.sp, fontWeight = FontWeight.Bold)
                    Icon(Icons.Default.Settings, contentDescription = "Settings", tint = Color(0xFF48D9FF))
                }
                Text("PERSONAL AI ASSISTANT", color = Color(0xFF72BFEA), fontSize = 11.sp, letterSpacing = 2.sp)
                Spacer(modifier = Modifier.size(12.dp))
                Box(
                    modifier = Modifier.size(220.dp).background(
                        Brush.radialGradient(listOf(Color(0xFF176B9A), Color(0xFF0B2745), Color.Transparent)),
                        CircleShape
                    ),
                    contentAlignment = Alignment.Center
                ) {
                    Button(
                        onClick = onMicClick,
                        modifier = Modifier.size(142.dp),
                        shape = CircleShape,
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (listening) Color(0xFF00A9D8) else Color(0xFF102E4D)
                        )
                    ) {
                        Icon(Icons.Default.Mic, contentDescription = "Voice", modifier = Modifier.size(54.dp), tint = Color.White)
                    }
                }
                Text(if (listening) "LISTENING…" else "SAY A COMMAND", color = Color(0xFF66DFFF), fontWeight = FontWeight.Bold)
                Text(statusText, color = Color(0xFFB3C9DA), fontSize = 13.sp)
                InfoCard("YOU SAID", heardText)
                InfoCard("JARVIS RESPONSE", response)
                Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    Button(onClick = onVolumeDown, colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF123B5C))) {
                        Icon(Icons.Default.VolumeDown, contentDescription = null)
                        Text("Volume −")
                    }
                    Button(onClick = onVolumeUp, colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF123B5C))) {
                        Icon(Icons.Default.VolumeUp, contentDescription = null)
                        Text("Volume +")
                    }
                }
                Spacer(modifier = Modifier.weight(1f))
                Text("MVP • Tap microphone to speak", color = Color(0xFF64839B), fontSize = 11.sp)
            }
        }
    }
}

@Composable
private fun InfoCard(title: String, value: String) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(containerColor = Color(0xCC10253A))
    ) {
        Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(title, color = Color(0xFF52CFF5), fontSize = 10.sp, fontWeight = FontWeight.Bold)
            Text(value, color = Color.White, fontSize = 14.sp)
        }
    }
}
