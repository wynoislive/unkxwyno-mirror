package io.unkxwyno.mirror.discovery

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.net.wifi.WifiManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

data class DiscoveredPeer(val instanceName: String, val host: String?, val port: Int)

sealed interface DiscoveryState {
    data object Idle : DiscoveryState
    data object Starting : DiscoveryState
    data object Active : DiscoveryState
    data object PermissionRequired : DiscoveryState
    data class Failed(val errorCode: Int?) : DiscoveryState
}

interface LocalDiscovery {
    val peers: StateFlow<List<DiscoveredPeer>>
    val state: StateFlow<DiscoveryState>
    fun start()
    fun stop()
}

class AndroidNsdDiscovery(context: Context) : LocalDiscovery {
    private val applicationContext = context.applicationContext
    private val manager = applicationContext.getSystemService(NsdManager::class.java)
    private val multicastLock = applicationContext.getSystemService(WifiManager::class.java)
        .createMulticastLock("unkxwyno-mdns")
        .apply { setReferenceCounted(false) }
    private val mutablePeers = MutableStateFlow<List<DiscoveredPeer>>(emptyList())
    override val peers: StateFlow<List<DiscoveredPeer>> = mutablePeers.asStateFlow()
    private val mutableState = MutableStateFlow<DiscoveryState>(DiscoveryState.Idle)
    override val state: StateFlow<DiscoveryState> = mutableState.asStateFlow()
    private var active = false

    private val listener = object : NsdManager.DiscoveryListener {
        override fun onDiscoveryStarted(serviceType: String) {
            mutableState.value = DiscoveryState.Active
        }

        override fun onDiscoveryStopped(serviceType: String) {
            active = false
            releaseMulticastLock()
            mutablePeers.value = emptyList()
            mutableState.value = DiscoveryState.Idle
        }

        override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
            active = false
            releaseMulticastLock()
            mutablePeers.value = emptyList()
            mutableState.value = DiscoveryState.Failed(errorCode)
        }

        override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
            active = false
            releaseMulticastLock()
            mutablePeers.value = emptyList()
            mutableState.value = DiscoveryState.Failed(errorCode)
        }

        override fun onServiceFound(serviceInfo: NsdServiceInfo) {
            if (!sameServiceType(serviceInfo.serviceType, SERVICE_TYPE)) return
            @Suppress("DEPRECATION")
            manager.resolveService(serviceInfo, object : NsdManager.ResolveListener {
                override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) = Unit
                override fun onServiceResolved(resolved: NsdServiceInfo) {
                    val peer = DiscoveredPeer(resolved.serviceName, resolved.host?.hostAddress, resolved.port)
                    mutablePeers.value = (mutablePeers.value.filterNot { it.instanceName == peer.instanceName } + peer)
                        .sortedBy { it.instanceName }
                }
            })
        }

        override fun onServiceLost(serviceInfo: NsdServiceInfo) {
            mutablePeers.value = mutablePeers.value.filterNot { it.instanceName == serviceInfo.serviceName }
        }
    }

    override fun start() {
        if (active) return
        if (!LocalNetworkPermission.isGranted(applicationContext)) {
            mutableState.value = DiscoveryState.PermissionRequired
            return
        }

        active = true
        mutableState.value = DiscoveryState.Starting
        try {
            if (!multicastLock.isHeld) multicastLock.acquire()
            manager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, listener)
        } catch (_: SecurityException) {
            active = false
            releaseMulticastLock()
            mutableState.value = DiscoveryState.PermissionRequired
        } catch (_: RuntimeException) {
            active = false
            releaseMulticastLock()
            mutableState.value = DiscoveryState.Failed(null)
        }
    }

    override fun stop() {
        if (!active) {
            releaseMulticastLock()
            mutablePeers.value = emptyList()
            mutableState.value = DiscoveryState.Idle
            return
        }
        active = false
        runCatching { manager.stopServiceDiscovery(listener) }
            .onFailure {
                releaseMulticastLock()
                mutableState.value = DiscoveryState.Failed(null)
            }
        mutablePeers.value = emptyList()
    }

    private fun releaseMulticastLock() {
        if (multicastLock.isHeld) multicastLock.release()
    }

    companion object {
        const val SERVICE_TYPE = "_unkxwyno._tcp."

        private fun sameServiceType(left: String, right: String): Boolean =
            left.trimEnd('.').equals(right.trimEnd('.'), ignoreCase = true)
    }
}
