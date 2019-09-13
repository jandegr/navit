package org.navitproject.navit;

import android.app.Application;
import android.content.SharedPreferences;
import android.content.res.Resources;

import java.util.ArrayList;
import java.util.List;


public class NavitAppConfig extends Application {

    private static final int         MAX_LAST_ADDRESSES = 10;
    //private static final String      TAG                = "Navit";

    private List<NavitSearchAddress> mLastAddresses     = null;
    private int                      mLastAddressField;
    private SharedPreferences        mSettings;
    private static Resources         resources;

    @Override
    public void onCreate() {
        super.onCreate();
        mSettings = getSharedPreferences(Navit.NAVIT_PREFS, MODE_PRIVATE);
        resources = getResources();
    }


    // Called when the operating system has determined that it is a good time
    // for a process to trim unneeded memory from its process.
    // If you override this method you must call through to the superclass implementation.
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
    }

    // This is called when the overall system is running low on memory,
    // and actively running processes should trim their memory usage.
    // If you override this method you must call through to the superclass implementation.
    public void onLowMemory() {
        super.onLowMemory();
    }

    // This method is for use in emulated process environments.
    public void onTerminate() {
        super.onTerminate();
    }

    List<NavitSearchAddress> getLastAddresses() {
        if (mLastAddresses == null) {
            mLastAddresses = new ArrayList<>();
            int lastAddressField = mSettings.getInt("LastAddress", -1);
            if (lastAddressField >= 0) {
                int index = lastAddressField;
                do {
                    String addrStr = mSettings.getString("LastAddress_" + String.valueOf(index), "");

                    if (addrStr.length() > 0) {
                        mLastAddresses.add(new NavitSearchAddress(
                                1,0,
                                mSettings.getFloat("LastAddress_Lat_" + String.valueOf(index), 0),
                                mSettings.getFloat("LastAddress_Lon_" + String.valueOf(index), 0),
                                addrStr, null));
                    }

                    if (--index < 0) {
                        index = MAX_LAST_ADDRESSES - 1;
                    }

                } while (index != lastAddressField);
            }
        }
        return mLastAddresses;
    }

    void addLastAddress(NavitSearchAddress newAddress) {
        getLastAddresses();

        mLastAddresses.add(newAddress);
        if (mLastAddresses.size() > MAX_LAST_ADDRESSES) {
            mLastAddresses.remove(0);
        }

        mLastAddressField++;
        if (mLastAddressField >= MAX_LAST_ADDRESSES) {
            mLastAddressField = 0;
        }

        SharedPreferences.Editor editSettings = mSettings.edit();

        editSettings.putInt("LastAddress", mLastAddressField);
        editSettings.putString("LastAddress_" + String.valueOf(mLastAddressField), newAddress.mAddr);
        editSettings.putFloat("LastAddress_Lat_" + String.valueOf(mLastAddressField), newAddress.mLat);
        editSettings.putFloat("LastAddress_Lon_" + String.valueOf(mLastAddressField), newAddress.mLon);

        editSettings.apply();
    }

    /**
     * Translates a string from its id in R.strings.
     *
     * @param riD resource identifier
     * @return translated string
     */
    static String getTstring(int riD) {

        return callbackLocalizedString(resources.getString(riD));
    }

    static native String callbackLocalizedString(String s);

}
