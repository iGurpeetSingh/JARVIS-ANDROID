package com.jarvis.assistant

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

class SettingsActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val prefs = getSharedPreferences("jarvis_settings", MODE_PRIVATE)
        setContent {
            var mode by remember { mutableStateOf(prefs.getString("ai_mode", "AUTO") ?: "AUTO") }
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize(), color = Color(0xFF050B16)) {
                    Column(
                        modifier = Modifier.fillMaxSize().background(Color(0xFF050B16)).padding(20.dp),
                        verticalArrangement = Arrangement.spacedBy(14.dp)
                    ) {
                        Text("JARVIS SETTINGS", color = Color(0xFF66DFFF), style = MaterialTheme.typography.headlineSmall)
                        Text("AI mode preference", color = Color.White, style = MaterialTheme.typography.titleMedium)
                        ModeChoice("AUTO", "Automatic: choose online when validated internet is available; otherwise use offline route.", mode) { mode = it }
                        ModeChoice("ONLINE", "Online only. Requires an online AI provider to be configured.", mode) { mode = it }
                        ModeChoice("OFFLINE", "Offline only. Requires a compatible local model to be installed.", mode) { mode = it }
                        Button(
                            onClick = { prefs.edit().putString("ai_mode", mode).apply(); finish() },
                            modifier = Modifier.fillMaxWidth()
                        ) { Text("Save preference") }
                        Card(colors = CardDefaults.cardColors(containerColor = Color(0xFF10253A))) {
                            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                                Text("Setup status", color = Color(0xFF66DFFF))
                                Text("This build includes the mode preference and network decision logic. Online provider/API and downloadable offline language-model runtime are not connected yet.", color = Color.White)
                                Text("No paid API is enabled or purchased by this app.", color = Color(0xFFB3C9DA))
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ModeChoice(value: String, description: String, selected: String, onSelect: (String) -> Unit) {
    Card(colors = CardDefaults.cardColors(containerColor = Color(0xFF10253A))) {
        Row(modifier = Modifier.fillMaxWidth().padding(10.dp)) {
            RadioButton(selected = selected == value, onClick = { onSelect(value) })
            Column(Modifier.padding(top = 8.dp)) {
                Text(value, color = Color.White)
                Text(description, color = Color(0xFFB3C9DA), style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}
