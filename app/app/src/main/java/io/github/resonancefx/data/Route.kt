// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

package io.github.resonancefx.data

import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Build

/** The output audio is currently routed to; each route gets its own profile. */
data class Route(val key: String, val label: String) {
    companion object {
        val SPEAKER = Route("speaker", "Speaker")

        /** Picks the output media would play on, preferring the most specific external device. */
        fun current(audio: AudioManager): Route {
            val devices = audio.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
            fun name(d: AudioDeviceInfo) = d.productName?.toString()?.trim().orEmpty().ifEmpty { "Unknown" }

            devices.firstOrNull { it.type in bluetoothTypes }?.let { return Route("bluetooth:${name(it)}", name(it)) }
            devices.firstOrNull { it.type in usbTypes }?.let { return Route("usb:${name(it)}", "USB · ${name(it)}") }
            devices.firstOrNull { it.type in wiredTypes }?.let { return Route("wired", "Wired headphones") }
            devices.firstOrNull { it.type == AudioDeviceInfo.TYPE_HDMI }?.let { return Route("hdmi", "HDMI") }
            return SPEAKER
        }

        private val bluetoothTypes = buildSet {
            add(AudioDeviceInfo.TYPE_BLUETOOTH_A2DP)
            if (Build.VERSION.SDK_INT >= 31) {
                add(AudioDeviceInfo.TYPE_BLE_HEADSET)
                add(AudioDeviceInfo.TYPE_BLE_SPEAKER)
            }
            if (Build.VERSION.SDK_INT >= 33) add(AudioDeviceInfo.TYPE_BLE_BROADCAST)
        }
        private val usbTypes = buildSet {
            add(AudioDeviceInfo.TYPE_USB_DEVICE)
            add(AudioDeviceInfo.TYPE_USB_ACCESSORY)
            if (Build.VERSION.SDK_INT >= 26) add(AudioDeviceInfo.TYPE_USB_HEADSET)
        }
        private val wiredTypes = setOf(AudioDeviceInfo.TYPE_WIRED_HEADPHONES, AudioDeviceInfo.TYPE_WIRED_HEADSET)
    }
}
