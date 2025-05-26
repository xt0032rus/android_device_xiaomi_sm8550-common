/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.xiaomi.settings.thermal;

import android.app.ActivityTaskManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.Log;
import androidx.preference.PreferenceManager;

import com.xiaomi.settings.utils.FileUtils;

import java.util.Map;

public final class ThermalUtils {

    private static final String TAG = "ThermalUtils";
    private static final String THERMAL_CONTROL = "thermal_control_v2";
    private static final String THERMAL_ENABLED = "thermal_enabled";
    private static final String LAST_MANUAL_THERMAL_PROFILE = "last_manual_thermal_profile";

    protected static final int STATE_DEFAULT = 0;
    protected static final int STATE_BATTERY = 1;
    protected static final int STATE_GAMING = 2;
    protected static final int STATE_PERFORMANCE = 3;

    protected static final Map<Integer, String> THERMAL_STATE_MAP = Map.of(
        STATE_DEFAULT, "0",
        STATE_BATTERY, "1",
        STATE_GAMING, "19",
        STATE_PERFORMANCE, "6"
    );

    private static final String THERMAL_BATTERY = "thermal.battery=";
    private static final String THERMAL_GAMING = "thermal.gaming=";
    private static final String THERMAL_DEFAULT = "thermal.default=";
    private static final String THERMAL_PERFORMANCE = "thermal.performance=";

    protected static final String THERMAL_SCONFIG = "/sys/class/thermal/thermal_message/sconfig";

    private Context mContext;
    private SharedPreferences mSharedPrefs;
    private Boolean mEnabled;
    private Intent mServiceIntent;

    private static ThermalUtils sInstance;

    private ThermalUtils(Context context) {
        mContext = context;
        mSharedPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        mEnabled = isEnabled();
        mServiceIntent = new Intent(context, ThermalService.class);
    }

    public static synchronized ThermalUtils getInstance(Context context) {
        if (sInstance == null) {
            sInstance = new ThermalUtils(context);
        }
        return sInstance;
    }

    public void startService() {
        if (mEnabled) {
            mContext.startServiceAsUser(mServiceIntent, UserHandle.CURRENT);
        }
    }

    private void stopService() {
        mContext.stopService(mServiceIntent);
    }

    public Boolean isEnabled() {
        return mSharedPrefs.getBoolean(THERMAL_ENABLED, false);
    }

    public void setEnabled(boolean enabled) {
        if (mEnabled == enabled) return;
        mEnabled = enabled;
        mSharedPrefs.edit().putBoolean(THERMAL_ENABLED, enabled).apply();
        if (enabled) {
            startService();
        } else {
            setDefaultThermalProfile();
            stopService();
        }
    }

    private void writeValue(String profiles) {
        mSharedPrefs.edit().putString(THERMAL_CONTROL, profiles).apply();
    }

    private String getValue() {
        String value = mSharedPrefs.getString(THERMAL_CONTROL, null);

        if (value == null || value.isEmpty() || value.split(":").length != 4) {
            Log.e(TAG, "Thermal control preferences are corrupted or empty. Resetting to default.");
            value = THERMAL_BATTERY + ":" + THERMAL_GAMING + ":" + THERMAL_DEFAULT + ":" + THERMAL_PERFORMANCE;
            writeValue(value);
        }
        return value;
    }

    protected void writePackage(String packageName, int mode) {
        String value = getValue();
        value = value.replace(packageName + ",", "");

        String[] modes = value.split(":");
        String finalString;

        switch (mode) {
            case STATE_BATTERY:
                modes[0] = modes[0] + packageName + ",";
                break;
            case STATE_GAMING:
                modes[1] = modes[1] + packageName + ",";
                break;
            case STATE_DEFAULT:
                modes[2] = modes[2] + packageName + ",";
                break;
            case STATE_PERFORMANCE:
                modes[3] = modes[3] + packageName + ",";
                break;
        }

        finalString = modes[0] + ":" + modes[1] + ":" + modes[2] + ":" + modes[3];

        writeValue(finalString);
    }

    protected int getStateForPackage(String packageName) {
        String value = getValue();
        String[] modes = value.split(":");
        int state = STATE_DEFAULT;

        if (modes[0].contains(packageName + ",")) {
            state = STATE_BATTERY;
        } else if (modes[1].contains(packageName + ",")) {
            state = STATE_GAMING;
        } else if (modes[2].contains(packageName + ",")) {
            state = STATE_DEFAULT;
        } else if (modes[3].contains(packageName + ",")) {
            state = STATE_PERFORMANCE;
        } else {
            state = getDefaultStateForPackage(packageName);
        }

        return state;
    }

    public void setDefaultThermalProfile() {
        FileUtils.writeLine(THERMAL_SCONFIG, THERMAL_STATE_MAP.get(STATE_DEFAULT));
    }

    public void setThermalProfile(String packageName) {
        if (!mEnabled) {
            return;
        }

        if (isPackageConfigured(packageName)) {
            final int state = getStateForPackage(packageName);
            FileUtils.writeLine(THERMAL_SCONFIG, THERMAL_STATE_MAP.get(state));
        } else {
            String currentGlobalProfile = FileUtils.readLine(THERMAL_SCONFIG);
            if (currentGlobalProfile != null && !currentGlobalProfile.isEmpty()) {
                FileUtils.writeLine(THERMAL_SCONFIG, currentGlobalProfile);
            } else {
                FileUtils.writeLine(THERMAL_SCONFIG, THERMAL_STATE_MAP.get(STATE_DEFAULT));
            }
        }
    }

    private int getDefaultStateForPackage(String packageName) {
        final PackageManager pm = mContext.getPackageManager();
        final ApplicationInfo appInfo;
        try {
            appInfo = pm.getApplicationInfo(packageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return STATE_DEFAULT;
        }

        switch (appInfo.category) {
            case ApplicationInfo.CATEGORY_GAME:
                return STATE_GAMING;
            case ApplicationInfo.CATEGORY_VIDEO:
                return STATE_BATTERY;
            default:
                return STATE_DEFAULT;
        }
    }

    public boolean isPackageConfigured(String packageName) {
        String value = getValue();
        String[] modes = value.split(":");
        return (modes[0].contains(packageName + ",") ||
                modes[1].contains(packageName + ",") ||
                modes[2].contains(packageName + ",") ||
                modes[3].contains(packageName + ","));
    }

    public String getCurrentAppPackageName() {
        try {
            final ActivityTaskManager.RootTaskInfo focusedTask =
                    ActivityTaskManager.getService().getFocusedRootTaskInfo();
            if (focusedTask != null && focusedTask.topActivity != null) {
                ComponentName taskComponentName = focusedTask.topActivity;
                return taskComponentName.getPackageName();
            }
        } catch (RemoteException e) {
        }
        return "";
    }

    public void setLastManualThermalProfile(int profile) {
        mSharedPrefs.edit().putInt(LAST_MANUAL_THERMAL_PROFILE, profile).apply();
    }

    public int getLastManualThermalProfile() {
        return mSharedPrefs.getInt(LAST_MANUAL_THERMAL_PROFILE, STATE_DEFAULT);
    }
}