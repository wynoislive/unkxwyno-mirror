/**
 * @file AoaAccessoryManager.kt
 * @brief Android Open Accessory (AOA 2.0) Manager for zero-dependency USB communication.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

package best.genzhost.unkxwynomirror.usb

import android.content.Context
import android.hardware.usb.UsbAccessory
import android.hardware.usb.UsbManager
import android.os.ParcelFileDescriptor
import android.util.Log
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException

class AoaAccessoryManager(private val context: Context) {
    private val TAG = "AoaAccessoryManager"
    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager

    private var fileDescriptor: ParcelFileDescriptor? = null
    var inputStream: FileInputStream? = null
        private set
    var outputStream: FileOutputStream? = null
        private set

    val isConnected: Boolean
        get() = fileDescriptor != null

    /**
     * Attempts to find and open an attached unkXwyno or compatible AOA accessory.
     */
    fun openAccessory(accessory: UsbAccessory? = null): Boolean {
        closeAccessory()

        val target = accessory ?: usbManager.accessoryList?.firstOrNull { acc ->
            acc.manufacturer == "unkXwyno" || acc.manufacturer == "XinDawn"
        }

        if (target == null) {
            Log.d(TAG, "No matching AOA accessory found.")
            return false
        }

        try {
            fileDescriptor = usbManager.openAccessory(target)
            if (fileDescriptor != null) {
                val fd = fileDescriptor!!.fileDescriptor
                inputStream = FileInputStream(fd)
                outputStream = FileOutputStream(fd)
                Log.i(TAG, "Successfully opened AOA accessory: ${target.manufacturer} - ${target.model}")
                return true
            } else {
                Log.e(TAG, "UsbManager.openAccessory returned null file descriptor.")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to open USB accessory: ${e.message}", e)
        }

        return false
    }

    /**
     * Closes current file descriptors and release USB streams.
     */
    fun closeAccessory() {
        try {
            inputStream?.close()
        } catch (_: IOException) {}
        try {
            outputStream?.close()
        } catch (_: IOException) {}
        try {
            fileDescriptor?.close()
        } catch (_: IOException) {}

        inputStream = null
        outputStream = null
        fileDescriptor = null
        Log.i(TAG, "AOA accessory connection closed.")
    }
}
