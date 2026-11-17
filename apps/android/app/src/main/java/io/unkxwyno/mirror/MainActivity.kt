package io.unkxwyno.mirror

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import io.unkxwyno.mirror.capture.CaptureState
import io.unkxwyno.mirror.capture.MediaProjectionCaptureController
import io.unkxwyno.mirror.capture.ProjectionSessionRegistry
import io.unkxwyno.mirror.discovery.LocalNetworkPermission

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(androidx.compose.ui.platform.ComposeView(this).apply {
            setContent { MaterialTheme { MainScreen() } }
        })
    }

    @Composable
    private fun MainScreen() {
        val controller = remember { MediaProjectionCaptureController(this) }
        val state by ProjectionSessionRegistry.state.collectAsState()
        var localNetworkGranted by remember { mutableStateOf(LocalNetworkPermission.isGranted(this)) }
        val consent = rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            controller.handleConsent(this, it.resultCode, it.data)
        }
        val localNetworkPermission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) {
            localNetworkGranted = LocalNetworkPermission.isGranted(this)
        }

        Scaffold(modifier = Modifier.fillMaxSize()) { padding ->
            Column(
                modifier = Modifier.padding(padding).padding(24.dp).fillMaxSize(),
                verticalArrangement = Arrangement.spacedBy(16.dp),
            ) {
                Text("UNKXWYNO Mirror", style = MaterialTheme.typography.headlineMedium)
                Text("Independent local screen mirroring", style = MaterialTheme.typography.bodyLarge)

                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(20.dp)) {
                        Text("Send this screen", style = MaterialTheme.typography.titleMedium)
                        Spacer(Modifier.height(8.dp))
                        Text(captureLabel(state))
                        Spacer(Modifier.height(12.dp))
                        Button(onClick = { consent.launch(controller.createConsentIntent()) }) {
                            Text("Grant screen capture permission")
                        }
                    }
                }

                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(20.dp)) {
                        Text("Receive a screen", style = MaterialTheme.typography.titleMedium)
                        Spacer(Modifier.height(8.dp))
                        Text("No authenticated peer is connected. Video decoding is not started until negotiation completes.")
                        if (LocalNetworkPermission.isRequired()) {
                            Spacer(Modifier.height(12.dp))
                            Text(
                                if (localNetworkGranted) {
                                    "Local-device access granted. Discovery still requires an explicit start."
                                } else {
                                    "Android 17 blocks local discovery until you grant local-device access."
                                },
                            )
                            if (!localNetworkGranted) {
                                Spacer(Modifier.height(8.dp))
                                Button(onClick = { localNetworkPermission.launch(LocalNetworkPermission.NAME) }) {
                                    Text("Allow local device access")
                                }
                            }
                        }
                    }
                }

                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("No account")
                    Text("No ads")
                    Text("Local only")
                }
            }
        }
    }

    private fun captureLabel(state: CaptureState): String = when (state) {
        CaptureState.Idle -> "Idle. Android has not granted capture access."
        CaptureState.AwaitingConsent -> "Waiting for Android's screen-capture consent dialog."
        CaptureState.AuthorizedAwaitingEncoder -> "Authorized. Waiting for a negotiated encoder surface; no frames are being sent."
        is CaptureState.Capturing -> "Capturing ${state.width} x ${state.height} at ${state.densityDpi} dpi."
        is CaptureState.Unavailable -> "Unavailable: ${state.reason}"
        is CaptureState.Failed -> "Capture failed: ${state.reason}"
    }
}
