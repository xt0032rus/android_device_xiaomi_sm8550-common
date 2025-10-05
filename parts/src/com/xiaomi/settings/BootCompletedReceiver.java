/*
 * Copyright (C) 2023 Paranoid Android
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.hardware.display.DisplayManager;
import android.os.Handler;
import android.os.UserHandle;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.Display.HdrCapabilities;

import androidx.preference.PreferenceManager;

import com.xiaomi.settings.autohbm.AutoHbmActivity;
import com.xiaomi.settings.autohbm.AutoHbmFragment;
import com.xiaomi.settings.autohbm.AutoHbmTileService;
import com.xiaomi.settings.thermal.ThermalService;
import com.xiaomi.settings.thermal.ThermalUtils;
import com.xiaomi.settings.touch.TouchOrientationService;
import com.xiaomi.settings.utils.ComponentUtils;

public class BootCompletedReceiver extends BroadcastReceiver {
    private static final String TAG = "XiaomiParts";
    private static final boolean DEBUG = true;

    @Override
    public void onReceive(final Context context, Intent intent) {
        if (!intent.getAction().equals(Intent.ACTION_BOOT_COMPLETED)) {
            return;
        }
        if (DEBUG) Log.d(TAG, "Received boot completed intent");
            
        PreferenceManager.setDefaultValues(context, R.xml.hypercharge_settings, false);
        try {
            ThermalUtils thermalUtils = ThermalUtils.getInstance(context);
            if (thermalUtils.isEnabled()) {
                Intent thermalServiceIntent = new Intent(context, ThermalService.class);
                context.startService(thermalServiceIntent);
                if (DEBUG) Log.d(TAG, "Started ThermalService");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to start ThermalService", e);
        }

        try {
            if (DEBUG) Log.d(TAG, "Starting Auto HBM service components");
            AutoHbmFragment.toggleAutoHbmService(context);
            ComponentUtils.toggleComponent(context, AutoHbmActivity.class, true);
            ComponentUtils.toggleComponent(context, AutoHbmTileService.class, true);
        } catch (Exception e) {
            Log.e(TAG, "Failed to start AutoHBM components", e);
        }

        try {
            SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
            boolean isHyperChargeEnabled = prefs.getBoolean(Constants.KEY_HYPERCHARGE_STATUS, true);

            if (DEBUG) Log.d(TAG, "HyperCharge state on boot: " + (isHyperChargeEnabled ? "ON" : "OFF (Limited)"));

            if (!isHyperChargeEnabled) {
                if (DEBUG) Log.d(TAG, "HyperCharge is set to OFF, starting limit service on boot.");
                Intent serviceIntent = new Intent(context, com.xiaomi.settings.hypercharge.HyperChargeService.class);
                context.startService(serviceIntent);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to start HyperChargeService", e);
        }

        try {
            if (DEBUG) Log.d(TAG, "Starting TouchOrientationService");
            // Touchscreen
            context.startServiceAsUser(new Intent(context, TouchOrientationService.class),
                    UserHandle.CURRENT);
        } catch (Exception e) {
            Log.e(TAG, "Failed to start TouchOrientationService", e);
        }

        try {
            com.xiaomi.settings.utils.GestureUtils.init(context);
            if (DEBUG) Log.d(TAG, "GestureUtils initialized at boot");
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize GestureUtils", e);
        }

        final DisplayManager displayManager = context.getSystemService(DisplayManager.class);
        displayManager.overrideHdrTypes(Display.DEFAULT_DISPLAY,
                new int[] {HdrCapabilities.HDR_TYPE_DOLBY_VISION, HdrCapabilities.HDR_TYPE_HDR10,
                        HdrCapabilities.HDR_TYPE_HLG, HdrCapabilities.HDR_TYPE_HDR10_PLUS});
    }
}