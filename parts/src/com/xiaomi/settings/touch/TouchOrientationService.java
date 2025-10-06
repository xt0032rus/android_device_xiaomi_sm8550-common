/*
 * Copyright (C) 2023 Paranoid Android
 * Copyright (C) 2025 TheMysticle
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.touch;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.hardware.display.DisplayManager;
import android.os.IBinder;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

import androidx.preference.PreferenceManager;

import com.xiaomi.settings.Constants;
import com.xiaomi.settings.utils.FileUtils;

public class TouchOrientationService extends Service {

    private static final String TAG = "XiaomiPartsTouchOrientationService";
    private static final boolean DEBUG = true;

    private DisplayManager mDisplayManager;
    private DisplayManager.DisplayListener mDisplayListener;

    @Override
    public void onCreate() {
        super.onCreate();
        if (DEBUG) Log.d(TAG, "Creating service");

        mDisplayManager = (DisplayManager) getSystemService(Context.DISPLAY_SERVICE);

        // Register a listener for orientation changes
        mDisplayListener = new DisplayManager.DisplayListener() {
            @Override
            public void onDisplayAdded(int displayId) {}

            @Override
            public void onDisplayRemoved(int displayId) {}

            @Override
            public void onDisplayChanged(int displayId) {
                if (displayId == Display.DEFAULT_DISPLAY) {
                    if (DEBUG) Log.d(TAG, "Display changed, updating orientation");
                    updateOrientation();
                }
            }
        };

        if (mDisplayManager != null) {
            mDisplayManager.registerDisplayListener(mDisplayListener, null);
        }

    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (DEBUG) Log.d(TAG, "onStartCommand");

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        boolean isEdgeRejectionEnabled = prefs.getBoolean(Constants.KEY_EDGE_REJECTION, true);

        if (!isEdgeRejectionEnabled) {
            if (DEBUG) Log.d(TAG, "Edge rejection is disabled, stopping service.");
            stopSelf();
            return START_NOT_STICKY;
        }

        updateOrientation();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        if (DEBUG) Log.d(TAG, "Service destroyed, cleaning up listeners.");
        if (mDisplayManager != null && mDisplayListener != null) {
            mDisplayManager.unregisterDisplayListener(mDisplayListener);
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void updateOrientation() {
        WindowManager wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        if (wm == null) {
            Log.e(TAG, "WindowManager is null, cannot update orientation");
            return;
        }

        int rotation = wm.getDefaultDisplay().getRotation();
        if (DEBUG) Log.d(TAG, "updateTpOrientation: rotation=" + rotation);

        boolean success = FileUtils.writeLine("/sys/class/touch/touch_dev/panel_orientation", rotation);
        if (DEBUG) {
            if (success) {
                Log.d(TAG, "Successfully wrote orientation " + rotation + " to panel_orientation");
            } else {
                Log.e(TAG, "Failed to write orientation to panel_orientation");
            }
        }
    }
}