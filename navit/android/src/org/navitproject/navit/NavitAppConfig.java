package org.navitproject.navit;

import android.app.Application;
import android.content.SharedPreferences;

import java.util.ArrayList;
import java.util.List;

import org.navitproject.navit.NavitAddressSearchActivity.NavitAddress;

public class NavitAppConfig extends Application {

    private static final int         MAX_LAST_ADDRESSES = 10;

    private List<NavitAddress> mLastAddresses     = null;
    private int                      mLastAddressField;
    private SharedPreferences        mSettings;

    @Override
    public void onCreate() {
        mSettings = getSharedPreferences(Navit.NAVIT_PREFS, MODE_PRIVATE);
        super.onCreate();
    }

    List<NavitAddress> getLastAddresses() {
        if (mLastAddresses == null) {
            mLastAddresses = new ArrayList<>();
            int mLastAddressField = mSettings.getInt("LastAddress", -1);
            if (mLastAddressField >= 0) {
                int index = mLastAddressField;
                do {
                    String addrStr = mSettings.getString("LastAddress_" + index, "");

                    if (addrStr.length() > 0) {
                        mLastAddresses.add(new NavitAddress(
                                               1,
                                               mSettings.getFloat("LastAddress_Lat_" + index, 0),
                                               mSettings.getFloat("LastAddress_Lon_" + index, 0),
                                               addrStr));
                    }

                    if (--index < 0) {
                        index = MAX_LAST_ADDRESSES - 1;
                    }

                } while (index != mLastAddressField);
            }
        }
        return mLastAddresses;
    }

    void addLastAddress(NavitAddress newAddress) {
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
        editSettings.putString("LastAddress_" + mLastAddressField, newAddress.mAddr);
        editSettings.putFloat("LastAddress_Lat_" + mLastAddressField, newAddress.mLat);
        editSettings.putFloat("LastAddress_Lon_" + mLastAddressField, newAddress.mLon);

        editSettings.apply();
    }
}
