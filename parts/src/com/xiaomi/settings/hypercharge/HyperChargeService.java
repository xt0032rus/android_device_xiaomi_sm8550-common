/*
 * Copyright (C) 2025 TheMysticle
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.hypercharge;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import com.xiaomi.settings.Constants;
import com.xiaomi.settings.utils.FileUtils;

public class HyperChargeService extends Service {

    private static final String TAG = "HyperChargeService";
    private static final boolean DEBUG = true;

    private static final String CURRENT_LIMIT_VALUE = "6000000";
    private static final int POLLING_INTERVAL_MS = 5000;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private Runnable mMonitoringRunnable;

    private final BroadcastReceiver powerReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (Intent.ACTION_POWER_CONNECTED.equals(action)) {
                Log.i(TAG, "Power connected — starting current enforcement loop.");
                startMonitoring();
            } else if (Intent.ACTION_POWER_DISCONNECTED.equals(action)) {
                Log.i(TAG, "Power disconnected — stopping current enforcement loop.");
                stopMonitoring();
            }
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        if (DEBUG) Log.d(TAG, "HyperCharge Limiter Service created.");

        // Register power state change receiver
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_POWER_CONNECTED);
        filter.addAction(Intent.ACTION_POWER_DISCONNECTED);
        registerReceiver(powerReceiver, filter);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (DEBUG) Log.d(TAG, "Service started.");
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        if (DEBUG) Log.d(TAG, "Service destroyed. Cleaning up.");
        unregisterReceiver(powerReceiver);
        stopMonitoring();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void startMonitoring() {
        stopMonitoring(); // In case it’s already running

        mMonitoringRunnable = new Runnable() {
            @Override
            public void run() {
                String currentValue = FileUtils.readOneLine(Constants.NODE_CONSTANT_CHARGE_CURRENT);
                if (DEBUG) Log.d(TAG, "Enforcing limit: Current value = " + currentValue + ", Target = " + CURRENT_LIMIT_VALUE);

                if (!CURRENT_LIMIT_VALUE.equals(currentValue)) {
                    Log.i(TAG, "Mismatch detected — applying charge current limit.");
                    applyCurrentLimit();
                }

                mHandler.postDelayed(this, POLLING_INTERVAL_MS);
            }
        };

        mHandler.post(mMonitoringRunnable);
    }

    private void stopMonitoring() {
        if (mMonitoringRunnable != null) {
            mHandler.removeCallbacks(mMonitoringRunnable);
            mMonitoringRunnable = null;
        }
    }

    private void applyCurrentLimit() {
        if (FileUtils.fileExists(Constants.NODE_CONSTANT_CHARGE_CURRENT)) {
            FileUtils.writeLine(Constants.NODE_CONSTANT_CHARGE_CURRENT, CURRENT_LIMIT_VALUE);
        } else {
            if (DEBUG) Log.e(TAG, "Sysfs node not found: " + Constants.NODE_CONSTANT_CHARGE_CURRENT);
        }
    }
}
