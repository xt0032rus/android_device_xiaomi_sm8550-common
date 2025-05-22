/*
 * Copyright (C) 2024 Paranoid Android
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.thermal

import android.content.ComponentName
import android.content.Intent
import android.os.Build
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService

import com.xiaomi.settings.R
import com.xiaomi.settings.utils.FileUtils

class ThermalTileService : TileService() {

    companion object {
        private const val THERMAL_PROFILE_PATH = "/sys/class/thermal/thermal_message/sconfig"
        private const val THERMAL_PROFILE_DEFAULT = 0
        private const val THERMAL_PROFILE_MBATTERY = 1
        private const val THERMAL_PROFILE_MGAME = 19
    }

    private fun updateTileUI(profile: Int) {
        val tile = qsTile
        tile.label = getString(R.string.thermalprofile_title)
        tile.subtitle = when (profile) {
            THERMAL_PROFILE_DEFAULT -> getString(R.string.thermalprofile_default)
            THERMAL_PROFILE_MBATTERY -> getString(R.string.thermalprofile_battery)
            THERMAL_PROFILE_MGAME -> getString(R.string.thermalprofile_game)
            else -> getString(R.string.thermalprofile_unknown)
        }
        tile.state = Tile.STATE_ACTIVE
        tile.updateTile()
    }

    override fun onStartListening() {
        super.onStartListening()

        val thermalUtils = ThermalUtils.getInstance(this)
        val tile = qsTile
        if (thermalUtils.isEnabled()) {
            val currentApp = thermalUtils.currentAppPackageName
            if (thermalUtils.isPackageConfigured(currentApp)) {
                tile.state = Tile.STATE_UNAVAILABLE
                tile.subtitle = getString(R.string.thermal_tile_disabled_subtitle)
            } else {
                updateTileUI(FileUtils.readLineInt(THERMAL_PROFILE_PATH))
            }
        } else {
            updateTileUI(FileUtils.readLineInt(THERMAL_PROFILE_PATH))
        }
        tile.updateTile()
    }

    override fun onStopListening() {
        super.onStopListening()
    }

    override fun onClick() {
        super.onClick()
        val thermalUtils = ThermalUtils.getInstance(this)

        if (thermalUtils.isEnabled()) {
            val currentApp = thermalUtils.currentAppPackageName
            if (thermalUtils.isPackageConfigured(currentApp)) {
                return
            }
        }

        val currentThermalProfile = FileUtils.readLineInt(THERMAL_PROFILE_PATH)
        val newThermalProfile = when (currentThermalProfile) {
            THERMAL_PROFILE_DEFAULT -> THERMAL_PROFILE_MBATTERY
            THERMAL_PROFILE_MBATTERY -> THERMAL_PROFILE_MGAME
            THERMAL_PROFILE_MGAME -> THERMAL_PROFILE_DEFAULT
            else -> THERMAL_PROFILE_DEFAULT
        }
        FileUtils.writeLine(THERMAL_PROFILE_PATH, newThermalProfile)
        updateTileUI(newThermalProfile)

        thermalUtils.setLastManualThermalProfile(
            when (newThermalProfile) {
                THERMAL_PROFILE_MBATTERY -> ThermalUtils.STATE_BATTERY
                THERMAL_PROFILE_MGAME -> ThermalUtils.STATE_GAMING
                else -> ThermalUtils.STATE_DEFAULT
            }
        )
    }
}