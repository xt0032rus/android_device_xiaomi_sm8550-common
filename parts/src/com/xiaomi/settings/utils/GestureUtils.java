package com.xiaomi.settings.utils;

import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.provider.Settings;
import android.util.Log;

import com.xiaomi.settings.utils.FileUtils;

public class GestureUtils {
    private static final String TAG = "GestureUtils";
    private static final boolean DEBUG = true;

    private static final String NODE_DT2W = "/sys/class/touch/touch_dev/gesture_double_tap_enabled";
    private static final String NODE_ST2W = "/sys/class/touch/touch_dev/gesture_single_tap_enabled";
    private static final String NODE_FODLP = "/sys/class/touch/touch_dev/fod_longpress_gesture_enabled";

    private GestureUtils() {
        // Utility class; prevent instantiation
    }

    public static void init(Context context) {
        ContentResolver resolver = context.getContentResolver();
        Handler handler = new Handler();

        // Register observers
        registerObserver(resolver, handler, "doze_pulse_on_double_tap");
        registerObserver(resolver, handler, "doze_tap_gesture");
        registerObserver(resolver, handler, "screen_off_udfps_enabled");

        // Sync once on boot
        updateNodes(resolver);
    }

    private static void registerObserver(ContentResolver resolver, Handler handler, String key) {
        Uri uri = Settings.Secure.getUriFor(key);
        resolver.registerContentObserver(uri, false, new ContentObserver(handler) {
            @Override
            public void onChange(boolean selfChange, Uri changedUri) {
                if (DEBUG) Log.d(TAG, "Setting changed: " + key);
                updateNodes(resolver);
            }
        });
    }

    private static void updateNodes(ContentResolver resolver) {
        int dt2w = Settings.Secure.getInt(resolver, "doze_pulse_on_double_tap", 0);
        int st2w = Settings.Secure.getInt(resolver, "doze_tap_gesture", 0);
        int fodlp = Settings.Secure.getInt(resolver, "screen_off_udfps_enabled", 0);

        writeNode(NODE_DT2W, dt2w);
        writeNode(NODE_ST2W, st2w);
        writeNode(NODE_FODLP, fodlp);
    }

    private static void writeNode(String path, int value) {
        if (!FileUtils.fileExists(path)) {
            if (DEBUG) Log.w(TAG, "Node does not exist: " + path);
            return;
        }
        boolean success = FileUtils.writeValue(path, String.valueOf(value));
        if (DEBUG) Log.d(TAG, "Wrote " + value + " to " + path + " success=" + success);
    }
}
