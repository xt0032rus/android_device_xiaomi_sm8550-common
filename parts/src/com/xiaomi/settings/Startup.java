/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.xiaomi.settings;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;
import androidx.preference.PreferenceManager;

import com.xiaomi.settings.Constants;
import com.xiaomi.settings.autohbm.AutoHbmActivity;
import com.xiaomi.settings.autohbm.AutoHbmFragment;
import com.xiaomi.settings.autohbm.AutoHbmTileService;
import com.xiaomi.settings.utils.ComponentUtils;
import com.xiaomi.settings.utils.FileUtils;

public class Startup extends BroadcastReceiver {

    private static final String TAG = "Startup";

    @Override
    public void onReceive(Context context, Intent intent) {
        final String action = intent.getAction();
        Log.d(TAG, "onReceive called with action: " + action);

        if (Intent.ACTION_BOOT_COMPLETED.equals(action) || 
            Intent.ACTION_REBOOT.equals(action)) {

            // Adding a delay before applying the settings
            new Handler(Looper.getMainLooper()).postDelayed(() -> {
                Log.d(TAG, "Applying saved settings...");
                applyAutoHbmSettings(context);
            }, 5000); // Delay of 5 seconds
        }
    }

    private void applyAutoHbmSettings(Context context) {
        Log.d(TAG, "Applying Auto HBM settings...");
        AutoHbmFragment.toggleAutoHbmService(context);

        ComponentUtils.toggleComponent(
                context,
                AutoHbmActivity.class,
                true
        );

        ComponentUtils.toggleComponent(
                context,
                AutoHbmTileService.class,
                true
        );
        Log.d(TAG, "Auto HBM settings applied");
    }
}
