package io.unkxwyno.mirror.discovery

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat

object LocalNetworkPermission {
    const val NAME = "android.permission.ACCESS_LOCAL_NETWORK"

    fun isRequired(): Boolean = Build.VERSION.SDK_INT >= 37

    fun isGranted(context: Context): Boolean = !isRequired() ||
        ContextCompat.checkSelfPermission(context, NAME) == PackageManager.PERMISSION_GRANTED
}
