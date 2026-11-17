package io.unkxwyno.mirror.discovery

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

sealed interface AdvertisementState {
    data object Idle : AdvertisementState
    data object Registering : AdvertisementState
    data class Registered(val instanceName: String, val port: Int) : AdvertisementState
    data object PermissionRequired : AdvertisementState
    data class Failed(val reason: String, val errorCode: Int? = null) : AdvertisementState
}

interface LocalServiceAdvertiser {
    val state: StateFlow<AdvertisementState>
    fun register(instanceName: String, port: Int)
    fun unregister()
}

class AndroidNsdAdvertiser(context: Context) : LocalServiceAdvertiser {
    private val applicationContext = context.applicationContext
    private val manager = applicationContext.getSystemService(NsdManager::class.java)
    private val mutableState = MutableStateFlow<AdvertisementState>(AdvertisementState.Idle)
    override val state: StateFlow<AdvertisementState> = mutableState.asStateFlow()
    private var listener: NsdManager.RegistrationListener? = null

    override fun register(instanceName: String, port: Int) {
        require(instanceName.isNotBlank()) { "Instance name cannot be blank." }
        require(port in 1..65535) { "Port must be between 1 and 65535." }
        unregister()
        if (!LocalNetworkPermission.isGranted(applicationContext)) {
            mutableState.value = AdvertisementState.PermissionRequired
            return
        }

        val registration = object : NsdManager.RegistrationListener {
            override fun onServiceRegistered(serviceInfo: NsdServiceInfo) {
                mutableState.value = AdvertisementState.Registered(serviceInfo.serviceName, serviceInfo.port)
            }

            override fun onRegistrationFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
                mutableState.value = AdvertisementState.Failed("DNS-SD registration failed.", errorCode)
                listener = null
            }

            override fun onServiceUnregistered(serviceInfo: NsdServiceInfo) {
                mutableState.value = AdvertisementState.Idle
                listener = null
            }

            override fun onUnregistrationFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
                mutableState.value = AdvertisementState.Failed("DNS-SD unregistration failed.", errorCode)
            }
        }

        listener = registration
        mutableState.value = AdvertisementState.Registering
        val service = NsdServiceInfo().apply {
            serviceName = instanceName
            serviceType = AndroidNsdDiscovery.SERVICE_TYPE
            setPort(port)
            setAttribute("v", "1")
        }
        try {
            manager.registerService(service, NsdManager.PROTOCOL_DNS_SD, registration)
        } catch (_: SecurityException) {
            listener = null
            mutableState.value = AdvertisementState.PermissionRequired
        } catch (error: RuntimeException) {
            listener = null
            mutableState.value = AdvertisementState.Failed(error.message ?: "DNS-SD registration failed.")
        }
    }

    override fun unregister() {
        val active = listener ?: run {
            mutableState.value = AdvertisementState.Idle
            return
        }
        runCatching { manager.unregisterService(active) }
            .onFailure {
                listener = null
                mutableState.value = AdvertisementState.Failed(it.message ?: "DNS-SD unregistration failed.")
            }
    }
}
