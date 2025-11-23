/*
 * Copyright (C) 2025 TheMysticle
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.hypercharge;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.drawable.Icon;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;
import android.util.Log;
import androidx.preference.PreferenceManager;

import com.xiaomi.settings.Constants;
import com.xiaomi.settings.R;

public class HyperChargeTileService extends TileService {

    private static final String TAG = "HyperChargeTileService";

    private SharedPreferences mSharedPrefs;

    @Override
    public void onCreate() {
        super.onCreate();
        mSharedPrefs = PreferenceManager.getDefaultSharedPreferences(this);
    }

    @Override
    public void onStartListening() {
        super.onStartListening();
        updateTileState();
    }

    @Override
    public void onClick() {
        super.onClick();
        toggleState();
        updateTileState();
    }

    private void toggleState() {
        boolean currentState = mSharedPrefs.getBoolean(Constants.KEY_HYPERCHARGE_STATUS, true);
        boolean newState = !currentState;
        mSharedPrefs.edit().putBoolean(Constants.KEY_HYPERCHARGE_STATUS, newState).apply();

        Intent serviceIntent = new Intent(this, HyperChargeService.class);
        if (newState) {
            Log.d(TAG, "HyperCharge is ON, stopping limit service.");
            stopService(serviceIntent);
        } else {
            Log.d(TAG, "HyperCharge is OFF, starting limit service.");
            startService(serviceIntent);
        }
    }

    private void updateTileState() {
        boolean isActive = mSharedPrefs.getBoolean(Constants.KEY_HYPERCHARGE_STATUS, true);
        
        Tile tile = getQsTile();
        if (tile == null) {
            return;
        }

        // isActive being true means HyperCharge is ON
        if (isActive) {
            tile.setState(Tile.STATE_ACTIVE);
            tile.setLabel(getString(R.string.hypercharge_on));
            tile.setIcon(Icon.createWithResource(this, R.drawable.ic_qs_hypercharge_on));
        } else {
            tile.setState(Tile.STATE_INACTIVE);
            tile.setLabel(getString(R.string.hypercharge_off));
            tile.setIcon(Icon.createWithResource(this, R.drawable.ic_qs_hypercharge_off));
        }
        tile.updateTile();
    }
}