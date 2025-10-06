/*
 * Copyright (C) 2025 TheMysticle
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.touch;

import android.util.Log;

import com.xiaomi.settings.Constants;
import com.xiaomi.settings.utils.FileUtils;

public class TouchUtils {

    private static final String TAG = "XiaomiTouchUtils";
    private static final boolean DEBUG = true;

    /**
     * Toggles the grip rejection feature by writing to its sysfs node.
     * @param enabled true to enable grip rejection, false to disable.
     */
    public static void setEdgeRejectionEnabled(boolean enabled) {
        String value = enabled ? "1" : "0";
        if (DEBUG) Log.d(TAG, "Setting edge rejection to: " + value);

        if (FileUtils.fileExists(Constants.NODE_GRIP_REJECTION)) {
            FileUtils.writeLine(Constants.NODE_GRIP_REJECTION, value);
        } else {
            Log.e(TAG, "Sysfs node for grip rejection not found at: " + Constants.NODE_GRIP_REJECTION);
        }
    }
}