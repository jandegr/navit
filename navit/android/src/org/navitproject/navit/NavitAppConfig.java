package org.navitproject.navit;

import android.app.Application;
import android.content.SharedPreferences;
import android.content.res.Resources;

import java.util.ArrayList;
import java.util.List;

public class NavitAppConfig extends Application {

    public static final String       NAVIT_PREFS = "NavitPrefs";
    private static final int         MAX_LAST_ADDRESSES = 10;
    static Resources                 sResources;
    private List<NavitSearchAddress>       mLastAddresses     = null;
    private int                      mLastAddressField;
    static SharedPreferences sSettings;


    @Override
    public void onCreate() {
        super.onCreate();
        sSettings = getSharedPreferences(NAVIT_PREFS, MODE_PRIVATE);
        sResources = getResources();
    }

    List<NavitSearchAddress> getLastAddresses() {
        if (mLastAddresses == null) {
            mLastAddresses = new ArrayList<>();
            int mLastAddressField = sSettings.getInt("LastAddress", -1);
            if (mLastAddressField >= 0) {
                int index = mLastAddressField;
                do {
                    String addrStr = sSettings.getString("LastAddress_" + index, "");

                   // if (addrStr.length() > 0) {
                   //       mLastAddresses.add(new NavitSearchAddress(
                   //                              1,
                   //                              sSettings.getFloat("LastAddress_Lat_" + index, 0),
                   //                              sSettings.getFloat("LastAddress_Lon_" + index, 0),
                   //                              addrStr));
                   //   }

                    if (--index < 0) {
                        index = MAX_LAST_ADDRESSES - 1;
                    }

                } while (index != mLastAddressField);
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

        SharedPreferences.Editor editSettings = sSettings.edit();

        editSettings.putInt("LastAddress", mLastAddressField);
        editSettings.putString("LastAddress_" + mLastAddressField, newAddress.mAddr);
        editSettings.putFloat("LastAddress_Lat_" + mLastAddressField, newAddress.mLat);
        editSettings.putFloat("LastAddress_Lon_" + mLastAddressField, newAddress.mLon);

        editSettings.apply();
    }

    /**
     * Translates a string from its id
     * in R.strings
     *
     * @param riD resource identifier
     * @return translated string
     */
    static String getTstring(int riD) {

        return callbackLocalizedString(sResources.getString(riD));
    }

    /**
     * Translates a string.
     * DEPECRATED !!!
     *
     * @param string to translate
     * @return translated string
     */
    static String getTstring(String string) {

        return callbackLocalizedString(string);
    }

    static native String callbackLocalizedString(String s);

    /*
     * this is used to load the 'navit' native library on
     * application startup. The library has already been unpacked at
     * installation time by the package manager.
     */
    static {
        System.loadLibrary("navit");
    }
}
