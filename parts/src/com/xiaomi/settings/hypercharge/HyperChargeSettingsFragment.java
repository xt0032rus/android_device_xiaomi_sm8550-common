/*
 * Copyright (C) 2025 TheMysticle
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.hypercharge;

import android.content.Intent;
import android.os.Bundle;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragment;
import androidx.preference.SwitchPreference;

import com.android.settingslib.widget.MainSwitchPreference;
// REMOVED: import com.android.settingslib.widget.OnMainSwitchChangeListener;

import com.xiaomi.settings.Constants;
import com.xiaomi.settings.R;

public class HyperChargeSettingsFragment extends PreferenceFragment
        implements Preference.OnPreferenceChangeListener {

    private MainSwitchPreference mSwitch;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.hypercharge_settings);
        
        mSwitch = findPreference(Constants.KEY_HYPERCHARGE_STATUS);
        // The setOnPreferenceChangeListener method is correct for all preferences
        mSwitch.setOnPreferenceChangeListener(this);
    }

    // This is the correct method to override for the OnPreferenceChangeListener interface
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (preference.getKey().equals(Constants.KEY_HYPERCHARGE_STATUS)) {
            boolean isChecked = (Boolean) newValue;
            
            Intent serviceIntent = new Intent(getContext(), HyperChargeService.class);
            if (isChecked) {
                getContext().stopService(serviceIntent);
            } else { 
                getContext().startService(serviceIntent);
            }
            return true;
        }
        return false;
    }
}