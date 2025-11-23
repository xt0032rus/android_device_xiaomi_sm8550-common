/*
 * Copyright (C) 2025 TheMysticle
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.touch;

import android.content.Intent;
import android.os.Bundle;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragment;
import com.android.settingslib.widget.MainSwitchPreference;

import com.xiaomi.settings.Constants;
import com.xiaomi.settings.R;

public class EdgeRejectionSettingsFragment extends PreferenceFragment
        implements Preference.OnPreferenceChangeListener {

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.edge_rejection_settings);
        
        MainSwitchPreference edgeRejectionSwitch = findPreference(Constants.KEY_EDGE_REJECTION);
        edgeRejectionSwitch.setOnPreferenceChangeListener(this);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (Constants.KEY_EDGE_REJECTION.equals(preference.getKey())) {
            boolean isEnabled = (Boolean) newValue;
            TouchUtils.setEdgeRejectionEnabled(isEnabled);

            Intent serviceIntent = new Intent(getContext(), TouchOrientationService.class);
            if (isEnabled) {
                getContext().startService(serviceIntent);
            } else {
                getContext().stopService(serviceIntent);
            }
            
            return true;
        }
        return false;
    }
}