/*
 * Copyright (C) 2025 TheMysticle
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.hypercharge;

import android.os.Bundle;
import com.android.settingslib.collapsingtoolbar.CollapsingToolbarBaseActivity;

public class HyperChargeSettingsActivity extends CollapsingToolbarBaseActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getFragmentManager().beginTransaction().replace(com.android.settingslib.collapsingtoolbar.R.id.content_frame,
                new HyperChargeSettingsFragment(), "hypercharge").commit();
    }
}