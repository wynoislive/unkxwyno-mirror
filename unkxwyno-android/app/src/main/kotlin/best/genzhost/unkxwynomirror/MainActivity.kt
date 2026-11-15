/**
 * @file MainActivity.kt
 * @brief Main UI and permission orchestrator for unkXwyno Mirror Android Companion App.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

package best.genzhost.unkxwynomirror

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.hardware.usb.UsbAccessory
import android.hardware.usb.UsbManager
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Bundle
import android.util.DisplayMetrics
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import best.genzhost.unkxwynomirror.service.MirroringService
import best.genzhost.unkxwynomirror.ui.theme.*

class MainActivity : ComponentActivity() {

    private var isStreamingState = mutableStateOf(false)
    private var usbConnectedState = mutableStateOf(false)

    // Streaming parameters
    private var isWirelessMode by mutableStateOf(false)
    private var hostIpAddress by mutableStateOf("192.168.1.100")
    private var selectedFps by mutableIntStateOf(60)
    private var selectedBitrateMbps by mutableIntStateOf(25)
    private var isAudioEnabled by mutableStateOf(true)

    private val screenCaptureLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK && result.data != null) {
            startMirroringService(result.resultCode, result.data!!)
        } else {
            Toast.makeText(this, "Screen capture permission denied", Toast.LENGTH_SHORT).show()
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val recordAudioGranted = permissions[Manifest.permission.RECORD_AUDIO] ?: false
        if (!recordAudioGranted) {
            Toast.makeText(this, "Audio recording permission required for sound streaming", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        checkPermissions()
        checkUsbAccessory()

        setContent {
            UnkxwynoTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MirrorControlScreen(
                        isStreaming = isStreamingState.value,
                        isUsbConnected = usbConnectedState.value,
                        isWireless = isWirelessMode,
                        hostIp = hostIpAddress,
                        fps = selectedFps,
                        bitrateMbps = selectedBitrateMbps,
                        audioEnabled = isAudioEnabled,
                        onWirelessChange = { isWirelessMode = it },
                        onHostIpChange = { hostIpAddress = it },
                        onAutoDiscover = { discoverPcBeacon { hostIpAddress = it } },
                        onFpsChange = { selectedFps = it },
                        onBitrateChange = { selectedBitrateMbps = it },
                        onAudioToggle = { isAudioEnabled = it },
                        onToggleMirroring = { toggleMirroring() }
                    )
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        checkUsbAccessory()
    }

    private fun checkPermissions() {
        val permissions = mutableListOf(Manifest.permission.RECORD_AUDIO)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        }

        val ungranted = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (ungranted.isNotEmpty()) {
            permissionLauncher.launch(ungranted.toTypedArray())
        }
    }

    private fun checkUsbAccessory() {
        val usbManager = getSystemService(Context.USB_SERVICE) as UsbManager
        val accessories = usbManager.accessoryList
        usbConnectedState.value = accessories != null && accessories.isNotEmpty()
    }

    private fun toggleMirroring() {
        if (isStreamingState.value) {
            stopMirroringService()
        } else {
            val mpManager = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
            screenCaptureLauncher.launch(mpManager.createScreenCaptureIntent())
        }
    }

    private fun discoverPcBeacon(onDiscovered: (String) -> Unit) {
        Thread {
            try {
                val socket = java.net.DatagramSocket()
                socket.broadcast = true
                socket.soTimeout = 2500
                val msg = "UNKXWYNO_DISCOVERY_PING"
                val packet = java.net.DatagramPacket(msg.toByteArray(), msg.length, java.net.InetAddress.getByName("255.255.255.255"), 8098)
                socket.send(packet)

                val recvBuf = ByteArray(256)
                val recvPacket = java.net.DatagramPacket(recvBuf, recvBuf.size)
                socket.receive(recvPacket)
                val reply = String(recvPacket.data, 0, recvPacket.length)
                if (reply.contains("UNKXWYNO_DISCOVERY_PONG")) {
                    val ip = recvPacket.address.hostAddress ?: ""
                    runOnUiThread {
                        onDiscovered(ip)
                        Toast.makeText(this, "Discovered unkXwyno PC: $ip", Toast.LENGTH_SHORT).show()
                    }
                }
                socket.close()
            } catch (e: Exception) {
                runOnUiThread {
                    Toast.makeText(this, "Auto-discovery timed out. Enter IP manually.", Toast.LENGTH_SHORT).show()
                }
            }
        }.start()
    }

    private fun startMirroringService(resultCode: Int, data: Intent) {
        val metrics = resources.displayMetrics
        val width = 1920
        val height = 1080
        val dpi = metrics.densityDpi

        val serviceIntent = Intent(this, MirroringService::class.java).apply {
            action = MirroringService.ACTION_START
            putExtra(MirroringService.EXTRA_RESULT_CODE, resultCode)
            putExtra(MirroringService.EXTRA_RESULT_DATA, data)
            putExtra(MirroringService.EXTRA_WIDTH, width)
            putExtra(MirroringService.EXTRA_HEIGHT, height)
            putExtra(MirroringService.EXTRA_DPI, dpi)
            putExtra(MirroringService.EXTRA_BITRATE, selectedBitrateMbps * 1_000_000)
            putExtra(MirroringService.EXTRA_FPS, selectedFps)
            putExtra(MirroringService.EXTRA_AUDIO, isAudioEnabled)
            if (isWirelessMode) {
                putExtra(MirroringService.EXTRA_HOST_IP, hostIpAddress)
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent)
        } else {
            startService(serviceIntent)
        }

        isStreamingState.value = true
        Toast.makeText(this, "unkXwyno Mirror active!", Toast.LENGTH_SHORT).show()
    }

    private fun stopMirroringService() {
        val serviceIntent = Intent(this, MirroringService::class.java).apply {
            action = MirroringService.ACTION_STOP
        }
        startService(serviceIntent)
        isStreamingState.value = false
        Toast.makeText(this, "Mirroring stopped.", Toast.LENGTH_SHORT).show()
    }
}

@Composable
fun MirrorControlScreen(
    isStreaming: Boolean,
    isUsbConnected: Boolean,
    isWireless: Boolean,
    hostIp: String,
    fps: Int,
    bitrateMbps: Int,
    audioEnabled: Boolean,
    onWirelessChange: (Boolean) -> Unit,
    onHostIpChange: (String) -> Unit,
    onAutoDiscover: () -> Unit,
    onFpsChange: (Int) -> Unit,
    onBitrateChange: (Int) -> Unit,
    onAudioToggle: (Boolean) -> Unit,
    onToggleMirroring: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(20.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        // Header
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Spacer(modifier = Modifier.height(8.dp))
            Text(
                text = "unkXwyno Mirror",
                fontSize = 28.sp,
                fontWeight = FontWeight.Bold,
                color = BrandPrimary
            )
            Text(
                text = "Zero-Latency Ultra-HD Screen Mirroring",
                fontSize = 14.sp,
                color = TextSecondary
            )
            Spacer(modifier = Modifier.height(16.dp))

            // Connection Mode Selector
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.Center
            ) {
                FilterChip(
                    selected = !isWireless,
                    onClick = { onWirelessChange(false) },
                    label = { Text("🔌 USB AOA 2.0") },
                    modifier = Modifier.padding(end = 8.dp)
                )
                FilterChip(
                    selected = isWireless,
                    onClick = { onWirelessChange(true) },
                    label = { Text("📶 Wi-Fi 8099") }
                )
            }

            Spacer(modifier = Modifier.height(12.dp))

            // Wireless Host Input if Wi-Fi
            if (isWireless) {
                Card(
                    shape = RoundedCornerShape(12.dp),
                    colors = CardDefaults.cardColors(containerColor = SurfaceDark),
                    modifier = Modifier.fillMaxWidth().padding(bottom = 8.dp)
                ) {
                    Column(modifier = Modifier.padding(12.dp)) {
                        Text("Wireless PC Connection", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = BrandPrimary)
                        Spacer(modifier = Modifier.height(6.dp))
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            OutlinedTextField(
                                value = hostIp,
                                onValueChange = onHostIpChange,
                                singleLine = true,
                                modifier = Modifier.weight(1f),
                                label = { Text("PC IP Address", color = TextSecondary) }
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Button(
                                onClick = onAutoDiscover,
                                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF21262D))
                            ) {
                                Text("Scan", color = BrandPrimary)
                            }
                        }
                    }
                }
            }

            // Status Card
            Card(
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = SurfaceDark),
                modifier = Modifier.fillMaxWidth()
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Column {
                        val transportText = if (isWireless) "Wi-Fi Mode ($hostIp:8099)" else if (isUsbConnected) "USB Host: Connected" else "USB Host: Waiting for Cable..."
                        Text(
                            text = transportText,
                            fontWeight = FontWeight.SemiBold,
                            color = if (isWireless || isUsbConnected) StatusGreen else TextSecondary
                        )
                        Text(
                            text = if (isStreaming) "State: Streaming Live (1080p D3D11VA)" else "State: Ready to Mirror",
                            fontSize = 13.sp,
                            color = TextSecondary
                        )
                    }

                    Box(
                        modifier = Modifier
                            .size(14.dp)
                            .background(
                                color = if (isStreaming) StatusGreen else if (isWireless || isUsbConnected) BrandPrimary else Color.Gray,
                                shape = RoundedCornerShape(7.dp)
                            )
                    )
                }
            }
        }

        // Settings Section
        Column(modifier = Modifier.fillMaxWidth()) {
            Text("Encoding Parameters", fontWeight = FontWeight.Bold, color = TextPrimary)
            Spacer(modifier = Modifier.height(8.dp))

            // Framerate Selector
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("Target FPS: $fps", color = TextSecondary)
                Row {
                    listOf(60, 90, 120).forEach { rate ->
                        OutlinedButton(
                            onClick = { onFpsChange(rate) },
                            colors = ButtonDefaults.outlinedButtonColors(
                                contentColor = if (fps == rate) BrandPrimary else TextSecondary
                            ),
                            modifier = Modifier.padding(horizontal = 4.dp)
                        ) {
                            Text("$rate")
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(12.dp))

            // Bitrate Slider
            Text("Bitrate: $bitrateMbps Mbps", color = TextSecondary)
            Slider(
                value = bitrateMbps.toFloat(),
                onValueChange = { onBitrateChange(it.toInt()) },
                valueRange = 10f..60f,
                steps = 5,
                colors = SliderDefaults.colors(
                    thumbColor = BrandPrimary,
                    activeTrackColor = BrandPrimary
                )
            )

            Spacer(modifier = Modifier.height(8.dp))

            // Audio Capture Switch
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("Stream Internal Audio (48kHz)", color = TextSecondary)
                Switch(
                    checked = audioEnabled,
                    onCheckedChange = onAudioToggle,
                    colors = SwitchDefaults.colors(
                        checkedThumbColor = BrandPrimary,
                        checkedTrackColor = BrandSecondary
                    )
                )
            }
        }

        // Action Button
        Button(
            onClick = onToggleMirroring,
            modifier = Modifier
                .fillMaxWidth()
                .height(56.dp),
            shape = RoundedCornerShape(16.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = if (isStreaming) Color(0xFFFF5252) else BrandPrimary,
                contentColor = Color.Black
            )
        ) {
            Text(
                text = if (isStreaming) "Stop Mirroring" else "Start Ultra-Fast Mirroring",
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold
            )
        }
    }
}
