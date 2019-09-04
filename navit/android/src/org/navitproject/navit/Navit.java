/*
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

package org.navitproject.navit;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Application;
import android.app.Dialog;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Message;
import android.os.PowerManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.app.NotificationCompat;
import android.support.v4.content.ContextCompat;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.Toast;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Navit extends Activity {

    protected static NavitGraphics     graphics                        = null;
    private NavitDialogs               mDialogs;
    private PowerManager.WakeLock      mWakeLock;
    private NavitActivityResult[]      mActivityResults;
    public static InputMethodManager   mgr                             = null;
    public static DisplayMetrics       metrics                         = null;
    public static Boolean              show_soft_keyboard              = false;
    public static Boolean              show_soft_keyboard_now_showing  = false;
    public static long                 last_pressed_menu_key           = 0L;
    public static long                 time_pressed_menu_key           = 0L;
    private static Intent              startupIntent                   = null;
    private static long                startup_intent_timestamp        = 0L;
    private static final int           NavitDownloaderSelectMap_id     = 967;
    private static final int           NavitAddressSearch_id           = 70;
    private static final int           NavitSelectStorage_id           = 43;
    private static Resources           NavitResources                  = null;
    private static final String        CHANNEL_ID                      = "org.navitproject.navit";
    private static final String        NAVIT_PACKAGE_NAME              = "org.navitproject.navit";
    private static final String        TAG                             = "Navit";
    static String                      mapFilenamePath                 = null;
    static String                      NAVIT_DATA_DIR                  = null;
    public static final String         NAVIT_PREFS                     = "NavitPrefs";
    Boolean                            mIsFullscreen                   = false;
    private static final int           MY_PERMISSIONS_REQ_FINE_LOC     = 103;
    private static NotificationManager nm;
    private static Navit               navit                           = null;

    public static Navit getInstance() {
        return navit;
    }


    /**
     * @brief A Runnable to restore soft input when the user returns to the activity.
     *
     * An instance of this class can be passed to the main message queue in the Activity's
     * {@code onRestore()} method.
     */
    private class SoftInputRestorer implements Runnable {
        public void run() {
            Navit.this.showNativeKeyboard();
        }
    }

    private void createNotificationChannel() {
        /*
         * Create the NotificationChannel, but only on API 26+ because
         * the NotificationChannel class is new and not in the support library
         */
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            CharSequence name = getString(R.string.channel_name);
            //String description = getString(R.string.channel_description);
            int importance = NotificationManager.IMPORTANCE_LOW;
            NotificationChannel channel = new NotificationChannel(CHANNEL_ID, name, importance);
            //channel.setDescription(description);
            /*
             * Register the channel with the system; you can't change the importance
             * or other notification behaviors after this
             */
            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }
    }

    public void removeFileIfExists(String source) {
        File file = new File(source);

        if (!file.exists()) {
            return;
        }

        file.delete();
    }

    public void copyFileIfExists(String source, String destination) throws IOException {
        File file = new File(source);

        if (!file.exists()) {
            return;
        }

        FileInputStream is = null;
        FileOutputStream os = null;

        try {
            is = new FileInputStream(source);
            os = new FileOutputStream(destination);

            int len;
            byte[] buffer = new byte[1024];

            while ((len = is.read(buffer)) != -1) {
                os.write(buffer, 0, len);
            }
        } finally {
            /* Close the FileStreams to prevent Resource leaks */
            if (is != null) {
                is.close();
            }

            if (os != null) {
                os.close();
            }
        }
    }

    /**
     * Translates a string from its id
     * in R.strings
     *
     * @param riD resource identifier
     * @return translated string
     */
    String getTstring(int riD) {
        return getLocalizedString(getString(riD));
    }

    /**
     * Check if a specific file needs to be extracted from the apk archive.
     * This is based on whether the file already exist, and if so, whether it is older than the archive or not
     *
     * @param filename The full path to the file
     * @return true if file does not exist, but it can be created at the specified location, we will also return
     *         true if the file exist but the apk archive is more recent (probably package was upgraded)
     */
    private boolean resourceFileNeedsUpdate(String filename) {
        File resultfile = new File(filename);

        if (!resultfile.exists()) {
            File path = resultfile.getParentFile();
            if (!path.exists() && !resultfile.getParentFile().mkdirs()) {
                Log.e(TAG, "Could not create directory path for " + filename);
                return false;
            }
            return true;
        } else {
            PackageManager pm = getPackageManager();
            ApplicationInfo appInfo;
            long apkUpdateTime = 0;
            try {
                appInfo = pm.getApplicationInfo(NAVIT_PACKAGE_NAME, 0);
                apkUpdateTime = new File(appInfo.sourceDir).lastModified();
            } catch (NameNotFoundException e) {
                Log.e(TAG, "Could not read package infos");
                e.printStackTrace();
            }
            return apkUpdateTime > resultfile.lastModified();
        }
    }

    /**
     * Extract a resource from the apk archive (res/raw) and save it to a local file.
     *
     * @param result The full path to the local file
     * @param resname The name of the resource file in the archive
     * @return true if the local file is extracted in @p result
     */
    private boolean extractRes(String resname, String result) {
        Log.d(TAG, "Res Name " + resname + ", result " + result);
        int id = NavitResources.getIdentifier(resname, "raw", NAVIT_PACKAGE_NAME);
        Log.d(TAG, "Res ID " + id);
        if (id == 0) {
            return false;
        }

        if (resourceFileNeedsUpdate(result)) {
            Log.d(TAG, "Extracting resource");

            try {
                InputStream resourcestream = NavitResources.openRawResource(id);
                FileOutputStream resultfilestream = new FileOutputStream(new File(result));
                byte[] buf = new byte[1024];
                int i;
                while ((i = resourcestream.read(buf)) != -1) {
                    resultfilestream.write(buf, 0, i);
                }
                resultfilestream.close();
            } catch (Exception e) {
                Log.e(TAG, "Exception " + e.getMessage());
                return false;
            }
        }
        return true;
    }

    /**
     * Extract an asset from the apk archive (assets) and save it to a local file.
     *
     * @param output The full path to the output local file
     * @param assetFileName The full path of the asset file within the archive
     * @return true if the local file is extracted in @p output
     */
    private boolean extractAsset(String assetFileName, String output) {
        AssetManager assetMgr = NavitResources.getAssets();
        InputStream assetstream;
        Log.d(TAG, "Asset Name " + assetFileName + ", output " + output);
        try {
            assetstream = assetMgr.open(assetFileName);
        } catch (IOException e) {
            Log.e(TAG, "Failed opening asset '" + assetFileName + "'");
            return false;
        }

        if (resourceFileNeedsUpdate(output)) {
            Log.d(TAG, "Extracting asset '" + assetFileName + "'");

            try {
                FileOutputStream outputFilestream = new FileOutputStream(new File(output));
                byte[] buf = new byte[1024];
                int i;
                while ((i = assetstream.read(buf)) != -1) {
                    outputFilestream.write(buf, 0, i);
                }
                outputFilestream.close();
            } catch (Exception e) {
                Log.e(TAG, "Exception " + e.getMessage());
                return false;
            }
        }

        return true;
    }

    private void showInfos() {
        SharedPreferences settings = getSharedPreferences(NAVIT_PREFS, MODE_PRIVATE);
        boolean firstStart = settings.getBoolean("firstStart", true);

        if (firstStart) {
            AlertDialog.Builder infobox = new AlertDialog.Builder(this);
            infobox.setTitle(getTstring(R.string.initial_info_box_title)); // TRANS
            infobox.setCancelable(false);

            infobox.setMessage(getTstring(R.string.initial_info_box_message));

            infobox.setPositiveButton(getTstring(R.string.initial_info_box_OK), new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface arg0, int arg1) {
                    Log.d(TAG, "Ok, user saw the infobox");
                }
            });

            infobox.setNeutralButton(getTstring(R.string.initial_info_box_more_info),
                    new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface arg0, int arg1) {
                            Log.d(TAG, "user wants more info, show the website");
                            String url = "http://wiki.navit-project.org/index.php/Navit_on_Android";
                            Intent i = new Intent(Intent.ACTION_VIEW);
                            i.setData(Uri.parse(url));
                            startActivity(i);
                        }
                    });
            infobox.show();
            SharedPreferences.Editor preferenceEditor = settings.edit();
            preferenceEditor.putBoolean("firstStart", false);
            preferenceEditor.apply();
        }
    }

    private void verifyPermissions() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG,"ask for permission(s)");
            ActivityCompat.requestPermissions(this, new String[] {
                    Manifest.permission.ACCESS_FINE_LOCATION}, MY_PERMISSIONS_REQ_FINE_LOC);
        }
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.HONEYCOMB) {
            this.requestWindowFeature(Window.FEATURE_NO_TITLE);
        } else {
            this.getActionBar().hide();
        }

        mDialogs = new NavitDialogs(this);

        NavitResources = getResources();

        // only take arguments here, onResume gets called all the time (e.g. when screenblanks, etc.)
        Navit.startupIntent = this.getIntent();
        // hack! Remember time stamps, and only allow 4 secs. later in onResume to set target!
        Navit.startup_intent_timestamp = System.currentTimeMillis();
        Log.d(TAG, "**1**A " + startupIntent.getAction());
        Log.d(TAG, "**1**D " + startupIntent.getDataString());

        // NOTIFICATION
        // Setup the status bar notification
        // This notification is removed in the exit() function

        /* Whether this is the first launch of Navit (as opposed to the activity being recreated) */
        boolean isLaunch = (navit == null);
        if (isLaunch) {
            createNotificationChannel();
        }
        navit = this;
        buildNotification();
        verifyPermissions();
        // get the local language -------------
        Locale locale = java.util.Locale.getDefault();
        String lang = locale.getLanguage();
        String langc = lang;
        Log.d(TAG, "lang=" + lang);
        int pos = lang.indexOf('_');
        String navitLanguage;
        if (pos != -1) {
            langc = lang.substring(0, pos);
            navitLanguage = langc + lang.substring(pos).toUpperCase(locale);
            Log.d(TAG, "substring lang " + navitLanguage.substring(pos).toUpperCase(locale));
        } else {
            String country = locale.getCountry();
            Log.d(TAG, "Country1 " + country);
            Log.d(TAG, "Country2 " + country.toUpperCase(locale));
            navitLanguage = langc + "_" + country.toUpperCase(locale);
        }
        Log.d(TAG, "Language " + lang);

        SharedPreferences prefs = getSharedPreferences(NAVIT_PREFS,MODE_PRIVATE);
        NAVIT_DATA_DIR = getApplicationContext().getFilesDir().getPath();
        mapFilenamePath = prefs.getString("filenamePath", NAVIT_DATA_DIR + '/');
        Log.i(TAG,"NAVITDATADIR = " + NAVIT_DATA_DIR);
        Log.i(TAG,"mapFilenamePath = " + mapFilenamePath);
        // make sure the new path for the navitmap.bin file(s) exist!!
        File navitMapsDir = new File(mapFilenamePath);
        navitMapsDir.mkdirs();

        // make sure the share dir exists
        File navitDataShareDir = new File(NAVIT_DATA_DIR + "/share");
        navitDataShareDir.mkdirs();

        Display display = getWindowManager().getDefaultDisplay();
        int width = display.getWidth();
        int height = display.getHeight();
        metrics = new DisplayMetrics();
        display.getMetrics(Navit.metrics);
        int densityDpi = (int)((Navit.metrics.density * 160) - .5f);
        Log.d(TAG, "Navit -> pixels x=" + width + " pixels y=" + height);
        Log.d(TAG, "Navit -> dpi=" + densityDpi);
        Log.d(TAG, "Navit -> density=" + Navit.metrics.density);
        Log.d(TAG, "Navit -> scaledDensity=" + Navit.metrics.scaledDensity);

        mActivityResults = new NavitActivityResult[16];
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        mWakeLock = pm.newWakeLock(PowerManager.FULL_WAKE_LOCK | PowerManager.ON_AFTER_RELEASE,"navit:DoNotDimScreen");

        if (!extractRes(langc, NAVIT_DATA_DIR + "/locale/" + langc + "/LC_MESSAGES/navit.mo")) {
            Log.e(TAG, "Failed to extract language resource " + langc);
        }

        String myDisplayDensity;
        if (densityDpi <= 120) {
            myDisplayDensity = "ldpi";
        } else if (densityDpi <= 160) {
            myDisplayDensity = "mdpi";
        } else if (densityDpi < 240) {
            myDisplayDensity = "hdpi";
        } else if (densityDpi < 320) {
            myDisplayDensity = "xhdpi";
        } else if (densityDpi < 480) {
            myDisplayDensity = "xxhdpi";
        } else if (densityDpi < 640) {
            myDisplayDensity = "xxxhdpi";
        } else {
            Log.w(TAG, "found device of very high density (" + densityDpi + ")");
            Log.w(TAG, "using xxxhdpi values");
            myDisplayDensity = "xxxhdpi";
        }
        Log.i(TAG, "Device density detected: " + myDisplayDensity);

        try {
            AssetManager assetMgr = NavitResources.getAssets();
            String[] children = assetMgr.list("config/" + myDisplayDensity);
            for (String child : children) {
                Log.d(TAG, "Processing config file '" + child + "' from assets");
                if (!extractAsset("config/" + myDisplayDensity + "/" + child, NAVIT_DATA_DIR + "/share/" + child)) {
                    Log.e(TAG, "Failed to extract asset config/" + myDisplayDensity + "/" + child);
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to access assets using AssetManager");
        }
        Log.d(TAG, "android.os.Build.VERSION.SDK_INT=" + Integer.valueOf(android.os.Build.VERSION.SDK));
        navitMain(this, getApplication(), navitLanguage, Integer.valueOf(android.os.Build.VERSION.SDK),
                      myDisplayDensity, NAVIT_DATA_DIR + "/bin/navit", mapFilenamePath, isLaunch);
        if (graphics != null) {
            graphics.setmActivity(this);
        }

        showInfos();

        Navit.mgr = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    private void buildNotification() {
        nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);  // Grab a handle to the NotificationManager
        PendingIntent appIntent = PendingIntent.getActivity(getApplicationContext(), 0, getIntent(), 0);

        Notification navitNotification;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder builder;
            builder = new Notification.Builder(getApplicationContext(), CHANNEL_ID);
            builder.setContentIntent(appIntent);
            builder.setAutoCancel(false).setOngoing(true);
            builder.setContentTitle(getTstring(R.string.app_name));
            builder.setContentText(getTstring(R.string.notification_event_default));
            builder.setSmallIcon(R.drawable.ic_notify);
            navitNotification = builder.build();
        } else {
            NotificationCompat.Builder builder;
            builder = new NotificationCompat.Builder(getApplicationContext());
            builder.setContentIntent(appIntent);
            builder.setAutoCancel(false).setOngoing(true);
            builder.setContentTitle(getTstring(R.string.app_name));
            builder.setContentText(getTstring(R.string.notification_event_default));
            builder.setSmallIcon(R.drawable.ic_notify);
            navitNotification = builder.build();
        }
        nm.notify(R.string.app_name, navitNotification);// Show the notification
    }

    @Override
    public void onResume() {
        super.onResume();
        Log.d(TAG, "OnResume");
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            /* Required to make system bars fully transparent */
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
        //InputMethodManager mgr = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        // DEBUG
        // intent_data = "google.navigation:q=Wien Burggasse 27";
        // intent_data = "google.navigation:q=48.25676,16.643";
        // intent_data = "google.navigation:ll=48.25676,16.643&q=blabla-strasse";
        // intent_data = "google.navigation:ll=48.25676,16.643";
        if (startupIntent != null) {
            if (System.currentTimeMillis() <= Navit.startup_intent_timestamp + 4000L) {
                Log.d(TAG, "**2**A " + startupIntent.getAction());
                Log.d(TAG, "**2**D " + startupIntent.getDataString());
                String naviScheme = startupIntent.getScheme();
                if (naviScheme != null && naviScheme.equals("google.navigation")) {
                    parseNavigationURI(startupIntent.getData().getSchemeSpecificPart());
                }
            } else {
                Log.e(TAG, "timestamp for navigate_to expired! not using data");
            }
        }
        Log.d(TAG, "onResume");

        if (show_soft_keyboard_now_showing) {
            /* Calling showNativeKeyboard() directly won't work here, we need to use the message queue */
            View cf = getCurrentFocus();
            if (cf == null) {
                Log.e(TAG, "no view in focus, can't get a handler");
            } else {
                cf.getHandler().post(new SoftInputRestorer());
            }
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        Log.d(TAG, "onPause");
        if (show_soft_keyboard_now_showing) {
            Log.d(TAG, "onPause:hiding soft input");
            this.hideNativeKeyboard();
            show_soft_keyboard_now_showing = true;
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == MY_PERMISSIONS_REQ_FINE_LOC) {
            if (grantResults.length == 1 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                return;
            }
            AlertDialog.Builder infobox = new AlertDialog.Builder(this);
            infobox.setTitle(getTstring(R.string.permissions_info_box_title)); // TRANS
            infobox.setCancelable(false);
            infobox.setMessage(getTstring(R.string.permissions_not_granted));
            // TRANS
            infobox.setPositiveButton(getTstring(R.string.initial_info_box_OK),
                    new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface arg0, int arg1) {
                            exit();
                        }
                    });
            infobox.show();
        }
    }

    private void parseNavigationURI(String schemeSpecificPart) {
        String[] naviData = schemeSpecificPart.split("&");
        Pattern p = Pattern.compile("(.*)=(.*)");
        Map<String,String> params = new HashMap<>();
        for (String naviDatum : naviData) {
            Matcher m = p.matcher(naviDatum);
            if (m.matches()) {
                params.put(m.group(1), m.group(2));
            }
        }

        // d: google.navigation:q=blabla-strasse # (this happens when you are offline, or from contacts)
        // a: google.navigation:ll=48.25676,16.643&q=blabla-strasse
        // c: google.navigation:ll=48.25676,16.643
        // b: google.navigation:q=48.25676,16.643

        float lat;
        float lon;
        Bundle b = new Bundle();

        String geoString = params.get("ll");
        if (geoString != null) {
            String address = params.get("q");
            if (address != null) {
                b.putString("q", address);
            }
        } else {
            geoString = params.get("q");
        }

        if (geoString != null) {
            if (geoString.matches("^[+-]{0,1}\\d+(|\\.\\d*),[+-]{0,1}\\d+(|\\.\\d*)$")) {
                String[] geo = geoString.split(",");
                if (geo.length == 2) {
                    try {
                        lat = Float.valueOf(geo[0]);
                        lon = Float.valueOf(geo[1]);
                        b.putFloat("lat", lat);
                        b.putFloat("lon", lon);
                        Message msg = Message.obtain(mNavitGraphics.mCallbackHandler,
                                NavitGraphics.MsgType.CLB_SET_DESTINATION.ordinal());

                        msg.setData(b);
                        msg.sendToTarget();
                        Log.e(TAG, "target found (b): " + geoString);
                    } catch (NumberFormatException e) {
                        e.printStackTrace();
                    }
                }
            } else {
                start_targetsearch_from_intent(geoString);
            }
        }
    }

    public void setActivityResult(int requestCode, NavitActivityResult activityResult) {
        mActivityResults[requestCode] = activityResult;
    }


    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);
        //Log.e("Navit","onPrepareOptionsMenu");
        // this gets called every time the menu is opened!!
        // change menu items here!
        menu.clear();

        // group-id,item-id,sort order number
        //menu.add(1, 1, 100, getString(R.string.optionsmenu_zoom_in)); //TRANS
        //menu.add(1, 2, 200, getString(R.string.optionsmenu_zoom_out)); //TRANS

        menu.add(1, 3, 300, getTstring(R.string.optionsmenu_download_maps)); //TRANS
        menu.add(1, 5, 400, getTstring(R.string.optionsmenu_toggle_poi)); //TRANS

        menu.add(1, 6, 500, getTstring(R.string.optionsmenu_address_search)); //TRANS
        menu.add(1, 10, 600, getTstring(R.string.optionsmenu_set_map_location));

        menu.add(1, 99, 900, getTstring(R.string.optionsmenu_exit_navit)); //TRANS

        /* Only show the Backup to SD-Card Option if we really have one */
        if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            menu.add(1, 7, 700, getTstring(R.string.optionsmenu_backup_restore)); //TRANS
        }

        return true;
    }

    private NavitGraphics mNavitGraphics = null;

    public void setGraphics(NavitGraphics ng) {
        mNavitGraphics = ng;
    }

    public NavitGraphics getNavitGraphics() {
        return mNavitGraphics;
    }

    private void start_targetsearch_from_intent(String targetAddress) {
        if (targetAddress == null || targetAddress.equals("")) {
            // empty search string entered
            Toast.makeText(getApplicationContext(), getTstring(R.string.address_search_not_found),
                    Toast.LENGTH_LONG).show(); //TRANS
        } else {
            Intent searchIntent = new Intent(this, NavitAddressSearchActivity.class);
            searchIntent.putExtra("search_string", targetAddress);
            this.startActivityForResult(searchIntent, NavitAddressSearch_id);
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        runOptionsItem(item.getItemId());
        return true;
    }

    private void runOptionsItem(int id) {
        switch (id) {
            case 1 :
                // zoom in
                Message.obtain(mNavitGraphics.mCallbackHandler,
                        NavitGraphics.MsgType.CLB_ZOOM_IN.ordinal()).sendToTarget();
                // if we zoom, hide the bubble
                Log.d(TAG, "onOptionsItemSelected -> zoom in");
                break;
            case 2 :
                // zoom out
                Message.obtain(mNavitGraphics.mCallbackHandler,
                        NavitGraphics.MsgType.CLB_ZOOM_OUT.ordinal()).sendToTarget();
                // if we zoom, hide the bubble
                Log.d(TAG, "onOptionsItemSelected -> zoom out");
                break;
            case 3 :
                // map download menu
                Intent mapDownloadListActivity = new Intent(this, NavitDownloadSelectMapActivity.class);
                startActivityForResult(mapDownloadListActivity, Navit.NavitDownloaderSelectMap_id);
                break;
            case 5 :
                // toggle the normal POI layers and labels (to avoid double POIs)
                Message msg = Message.obtain(mNavitGraphics.mCallbackHandler,
                        NavitGraphics.MsgType.CLB_CALL_CMD.ordinal());
                Bundle b = new Bundle();
                b.putString("cmd", "toggle_layer(\"POI Symbols\");");
                msg.setData(b);
                msg.sendToTarget();

                msg = Message.obtain(mNavitGraphics.mCallbackHandler, NavitGraphics.MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "toggle_layer(\"POI Labels\");");
                msg.setData(b);
                msg.sendToTarget();

                // toggle full POI icons on/off
                msg = Message.obtain(mNavitGraphics.mCallbackHandler, NavitGraphics.MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "toggle_layer(\"Android-POI-Icons-full\");");
                msg.setData(b);
                msg.sendToTarget();

                break;
            case 6 :
                // ok startup address search activity
                Intent searchIntent = new Intent(this, NavitAddressSearchActivity.class);
                this.startActivityForResult(searchIntent, NavitAddressSearch_id);
                break;
            case 7 :
                /* Backup / Restore */
                showDialog(NavitDialogs.DIALOG_BACKUP_RESTORE);
                break;
            case 10:
                setMapLocation();
                break;
            case 99 :
                // exit
                this.onStop();
                this.exit();
                break;
            default:
                Log.e(TAG,"unhandled OptionsItem id = " + id);
        }
    }


    /**
     * Shows the Options menu.
     *
     * Calling this method has the same effect as pressing the hardware Menu button, where present, or touching
     * the overflow button in the Action bar.
     */
    public void showMenu() {
        openOptionsMenu();
    }


    /**
     * Shows the native keyboard or other input method.
     */
    int showNativeKeyboard() {
        /*
         * Apologies for the huge mess that this function is, but Android's soft input API is a big
         * nightmare. Its devs have mercifully given us an option to show or hide the keyboard, but
         * there is no reliable way to figure out if it is actually showing, let alone how much of the
         * screen it occupies, so our best bet is guesswork.
         */
        Configuration config = getResources().getConfiguration();
        if ((config.keyboard == Configuration.KEYBOARD_QWERTY)
                && (config.hardKeyboardHidden == Configuration.HARDKEYBOARDHIDDEN_NO)) {
            /* physical keyboard present, exit */
            return 0;
        }

        /* Use SHOW_FORCED here, else keyboard won't show in landscape mode */
        mgr.showSoftInput(getCurrentFocus(), InputMethodManager.SHOW_FORCED);
        show_soft_keyboard_now_showing = true;

        /*
         * Crude way to estimate the height occupied by the keyboard: for AOSP on KitKat and Lollipop it
         * is about 62-63% of available screen width (in portrait mode) but no more than slightly above
         * 46% of height (in landscape mode).
         */
        Display display = getWindowManager().getDefaultDisplay();
        int width = display.getWidth();
        int height = display.getHeight();
        int maxHeight = height * 47 / 100;
        int inputHeight = width * 63 / 100;
        if (inputHeight > (maxHeight)) {
            inputHeight = maxHeight;
        }

        /* the receiver isn't going to fire before the UI thread becomes idle, well after this method returns */
        Log.d(TAG, "showNativeKeyboard:return (assuming true)");
        return inputHeight;
    }


    /**
     * Hides the native keyboard or other input method.
     */
    void hideNativeKeyboard() {
        mgr.hideSoftInputFromWindow(getCurrentFocus().getWindowToken(), 0);
        show_soft_keyboard_now_showing = false;
    }


    void setDestination(float latitude, float longitude, String address) {
        Toast.makeText(getApplicationContext(),getTstring(R.string.address_search_set_destination) + "\n"
                + address, Toast.LENGTH_LONG).show(); //TRANS

        Bundle b = new Bundle();
        b.putFloat("lat", latitude);
        b.putFloat("lon", longitude);
        b.putString("q", address);
        Message msg = Message.obtain(mNavitGraphics.mCallbackHandler,
                NavitGraphics.MsgType.CLB_SET_DESTINATION.ordinal());
        msg.setData(b);
        msg.sendToTarget();
    }

    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case Navit.NavitDownloaderSelectMap_id :
                if (resultCode == Activity.RESULT_OK) {
                    Message msg = mDialogs.obtainMessage(NavitDialogs.MSG_START_MAP_DOWNLOAD,
                            data.getIntExtra("map_index", -1), 0);
                    msg.sendToTarget();
                }
                break;
            case NavitAddressSearch_id :
                if (resultCode == Activity.RESULT_OK) {
                    Bundle destination = data.getExtras();
                    Toast.makeText(getApplicationContext(),
                            getTstring(R.string.address_search_set_destination) + "\n" + destination.getString(("q")),
                            Toast.LENGTH_LONG).show(); //TRANS

                    Message msg = Message.obtain(mNavitGraphics.mCallbackHandler,
                            NavitGraphics.MsgType.CLB_SET_DESTINATION.ordinal());
                    msg.setData(destination);
                    msg.sendToTarget();
                }
                break;
            case NavitSelectStorage_id :
                if (resultCode == RESULT_OK) {
                    String newDir = data.getStringExtra(FileBrowserActivity.returnDirectoryParameter);
                    Log.d(TAG, "selected path= " + newDir);
                    if (!newDir.contains("/navit")) {
                        newDir = newDir + "/navit/";
                    } else {
                        newDir = newDir + "/";
                    }
                    SharedPreferences prefs = this.getSharedPreferences(NAVIT_PREFS,MODE_PRIVATE);
                    SharedPreferences.Editor  prefsEditor = prefs.edit();
                    prefsEditor.putString("filenamePath", newDir);
                    prefsEditor.apply();

                    Toast.makeText(this, String.format(getTstring(R.string.map_location_changed),newDir),
                            Toast.LENGTH_LONG).show();
                } else {
                    Log.w(TAG, "select path failed");
                }
                break;
            default :
                if (mActivityResults[requestCode] != null) {
                    mActivityResults[requestCode].onActivityResult(requestCode, resultCode, data);
                }
                break;
        }
    }

    @Override
    protected void onPrepareDialog(int id, Dialog dialog) {
        mDialogs.prepareDialog(id);
        super.onPrepareDialog(id, dialog);
    }

    protected Dialog onCreateDialog(int id) {
        return mDialogs.createDialog(id);
    }

    @Override
    public boolean onSearchRequested() {
        /* Launch the internal Search Activity */
        Intent searchIntent = new Intent(this, NavitAddressSearchActivity.class);
        this.startActivityForResult(searchIntent, NavitAddressSearch_id);

        return true;
    }

    private void setMapLocation() {
        Intent fileExploreIntent = new Intent(this,FileBrowserActivity.class);
        fileExploreIntent
            .putExtra(FileBrowserActivity.startDirectoryParameter, "/mnt")
            .setAction(FileBrowserActivity.INTENT_ACTION_SELECT_DIR);
        startActivityForResult(fileExploreIntent,NavitSelectStorage_id);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "OnDestroy");
    }

    void fullscreen(int fullscreen) {
        int width;
        int height;

        mIsFullscreen = (fullscreen != 0);
        if (mIsFullscreen) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        } else {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }

        Display display = getWindowManager().getDefaultDisplay();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1) {
            width = display.getWidth();
            height = display.getHeight();
        } else {
            Point size = new Point();
            display.getRealSize(size);
            width = size.x;
            height = size.y;
        }
        Log.d(TAG, String.format("Toggle fullscreen, w=%d, h=%d", width, height));
        mNavitGraphics.handleResize(width, height);
    }

    public void disableSuspend() {
        mWakeLock.acquire();
        mWakeLock.release();
    }

    private void exit() {
        nm.cancelAll();
        NavitVehicle.removeListeners(this.getApplicationContext());
        navitDestroy();
    }

    private native void navitMain(Navit x, Application application, String lang, int version,
            String displayDensityString, String path, String path2, boolean isLaunch);

    public native void navitDestroy();


    private String getLocalizedString(String text) {
        return NavitGraphics.callbackLocalizedString(text);
    }


    /*
     * this is used to load the 'navit' native library on
     * application startup. The library has already been unpacked at
     * installation time by the package manager.
     */
    static {
        System.loadLibrary("navit");
    }
}
