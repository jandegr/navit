package org.navitproject.navit;

import android.app.Application;
import android.content.SharedPreferences;
import java.util.ArrayList;
import java.util.List;

//@ReportsCrashes(
//      httpMethod = org.acra.sender.HttpSender.Method.PUT,
//          reportType = org.acra.sender.HttpSender.Type.JSON,
//          formUri = "http://192.168.1.115/acra-myapp/_design/acra-storage/_update/report",
//          formUriBasicAuthLogin = "testnavit",
//          formUriBasicAuthPassword = "p4ssw0rd")
//@ReportsCrashes(mailTo = "me@somewhere.com",
//      mode = ReportingInteractionMode.TOAST,
//                resToastText = R.string.app_name
//      )
public class NavitAppConfig extends Application {

    private static final int         MAX_LAST_ADDRESSES = 10;
    //private static final String      TAG                = "Navit";

    private List<NavitSearchAddress> mLastAddresses     = null;
    private int                      mLastAddressField;
    private SharedPreferences        mSettings;

    @Override
    public void onCreate() {
        // call ACRA.init(this) as reflection, because old ant may forgot to include it
        //      try {
        //          Class<?> acraClass = Class.forName("org.acra.ACRA");
        //          Class<?> partypes[] = new Class[1];
        //          partypes[0] = Application.class;
        //          java.lang.reflect.Method initMethod = acraClass.getMethod("init", partypes);
        //          Object arglist[] = new Object[1];
        //          arglist[0] = this;
        //          initMethod.invoke(null, arglist);
        //      } catch (Exception e1) {
        //          Log.e(TAG, "Could not init ACRA crash reporter");
        //      }

        mSettings = getSharedPreferences(Navit.NAVIT_PREFS, MODE_PRIVATE);
        super.onCreate();
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
}
