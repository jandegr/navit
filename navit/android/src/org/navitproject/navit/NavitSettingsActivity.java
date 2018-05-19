package org.navitproject.navit;


import android.os.Build;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.util.Log;
import java.io.File;



public class NavitSettingsActivity extends PreferenceActivity {

    private final String TAG = this.getClass().getName();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        PreferenceManager prefMgr = getPreferenceManager();
        prefMgr.setSharedPreferencesName("NavitPrefs");
        prefMgr.setSharedPreferencesMode(MODE_PRIVATE);
        setPreferenceScreen(createPreferenceHierarchy());
    }

    private PreferenceScreen createPreferenceHierarchy() {

        PreferenceScreen root = getPreferenceManager().createPreferenceScreen(this);

        if(android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            ListPreference listPref = new ListPreference(this);
            File[] navitDirs = getExternalFilesDirs(null);
            CharSequence[] entries = new CharSequence[navitDirs.length];
            CharSequence[] entryValues = new CharSequence[navitDirs.length];
            for (int i=0; i< navitDirs.length; i++){
                File f = navitDirs[i];
                entries[i] = f.toString(); // entries is the human readable form
                entryValues[i] = entries[i];
                Log.e(TAG,"candidate Dir "+ f );
            }
            listPref.setEntries(entries);
            listPref.setEntryValues(entryValues);
            listPref.setDialogTitle("map location");
            listPref.setKey("filenamePath");
            listPref.setTitle("map location");
            listPref.setSummary("choose where the maps will be stored");
            root.addPreference(listPref);
        }
        return root;
    }
}
