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

import static org.navitproject.navit.NavitAppConfig.getTstring;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Message;
import android.os.PowerManager;

import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;


public class Navit extends AppCompatActivity {


    public static boolean sShowSoftKeyboardShowing;
    private static final int MY_PERMISSIONS_REQ_FINE_LOC = 103;
    private static final int NavitDownloaderSelectMap_id = 967;
    private static final int NavitAddressSearch_id = 70;
    private static final int NavitSelectStorage_id = 43;
    private static final int MapsActivity_id = 56;
    private static final String NAVIT_PACKAGE_NAME = "org.navitproject.navit";
    private static final String TAG = "Navit";
    static String sMapFilenamePath;
    private boolean mIsFullscreen;
    private NavitDialogs mDialogs;
    private PowerManager.WakeLock mWakeLock;
    private NavitActivityResult[] mActivityResults;


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
            if (path != null && !path.exists() && !resultfile.getParentFile().mkdirs()) {
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
     * @param result  The full path to the local file
     * @param resname The name of the resource file in the archive
     * @return true if the local file is extracted in @p result
     */
    private boolean extractRes(String resname, String result) {
        Log.d(TAG, "Res Name " + resname + ", result " + result);
        int id = NavitAppConfig.sResources.getIdentifier(resname, "raw", NAVIT_PACKAGE_NAME);
        Log.d(TAG, "Res ID " + id);
        if (id == 0) {
            return false;
        }

        if (resourceFileNeedsUpdate(result)) {
            Log.d(TAG, "Extracting resource");

            try {
                InputStream resourcestream = NavitAppConfig.sResources.openRawResource(id);
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


    private void showInfos() {
        SharedPreferences settings = getSharedPreferences(NavitAppConfig.NAVIT_PREFS, MODE_PRIVATE);
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
            Log.d(TAG, "ask for permission(s)");
            ActivityCompat.requestPermissions(this, new String[]{
                    Manifest.permission.ACCESS_FINE_LOCATION}, MY_PERMISSIONS_REQ_FINE_LOC);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        windowSetup();
        mDialogs = new NavitDialogs(this);

        verifyPermissions();
        // get the local language -------------
        Locale locale = Locale.getDefault();
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

        SharedPreferences settings = getSharedPreferences(NavitAppConfig.NAVIT_PREFS, MODE_PRIVATE);
        String navitDataDir = getApplicationContext().getFilesDir().getPath();

        String candidateFileNamePath = getApplicationContext().getExternalFilesDir(null).toString();
        boolean firstStart = settings.getBoolean("firstStart", true);
        if (firstStart) {
            Log.e(TAG, "set to " + candidateFileNamePath);
            SharedPreferences.Editor preferenceEditor = settings.edit();
            preferenceEditor.putString("filenamePath", candidateFileNamePath);
            preferenceEditor.apply();
        }
        sMapFilenamePath = settings.getString("filenamePath", candidateFileNamePath);

        Log.i(TAG, "NavitDataDir = " + navitDataDir);
        Log.i(TAG, "mapFilenamePath = " + sMapFilenamePath);
        // make sure the new path for the navitmap.bin file(s) exist!!
        File navitMapsDir = new File(sMapFilenamePath);
        navitMapsDir.mkdirs();

        // make sure the share dir exists
        File navitDataShareDir = new File(navitDataDir + "/share");
        navitDataShareDir.mkdirs();

        Display display = getWindowManager().getDefaultDisplay();
        DisplayMetrics metrics = new DisplayMetrics();
        display.getMetrics(metrics);
        int densityDpi = (int) ((metrics.density * 160) - .5f);
        Log.d(TAG, "-> pixels x=" + display.getWidth() + " pixels y=" + display.getHeight());
        Log.d(TAG, "-> dpi=" + densityDpi);
        Log.d(TAG, "-> density=" + metrics.density);
        Log.d(TAG, "-> scaledDensity=" + metrics.scaledDensity);

        mActivityResults = new NavitActivityResult[16];
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        mWakeLock = pm.newWakeLock(PowerManager.FULL_WAKE_LOCK | PowerManager.ON_AFTER_RELEASE, "navit:DoNotDimScreen");

        if (!extractRes(langc, navitDataDir + "/locale/" + langc + "/LC_MESSAGES/navit.mo")) {
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

        if (!extractRes("navit" + myDisplayDensity, navitDataDir + "/share/navit.xml")) {
            Log.e("Navit", "Failed to extract navit.xml for " + myDisplayDensity);
        }

        Log.d(TAG, "android.os.Build.VERSION.SDK_INT=" + Build.VERSION.SDK_INT);
        navitMain(navitLanguage, navitDataDir + "/bin/navit", sMapFilenamePath + '/');
        showInfos();

        Intent startupIntent = new Intent(this.getIntent());
        Log.d(TAG, "onCreate intent " + startupIntent.toString());
        handleIntent(startupIntent);
    }

    private void handleIntent(Intent intent) {
        String naviScheme = intent.getScheme();
        if (naviScheme != null) {
            Log.d(TAG, "Using intent " + intent.toString());
            if (naviScheme.equals("google.navigation")) {
                parseNavigationURI(intent.getData().getSchemeSpecificPart());
            }
            //else if (naviScheme.equals("geo")
            //        && intent.getAction().equals("android.intent.action.VIEW")) {
            //    invokeCallbackOnGeo(intent.getData().getSchemeSpecificPart(),
            //            NavitCallbackHandler.MsgType.CLB_SET_DESTINATION, "");
            //}
        }
    }

    private void windowSetup() {
        //   if (this.getActionBar() != null) {
        //       this.getActionBar().hide();
        //   }
    }


    public void onRestart() {
        super.onRestart();
        Log.e(TAG, "onRestart");
    }

    public void onStart() {
        super.onStart();
        Log.e(TAG, "onStart");
    }

    @Override
    public void onResume() {
        super.onResume();
        Log.e(TAG, "onResume");
        CallBackHandler.sendCommand(CallBackHandler.CmdType.CMD_UNBLOCK);
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            /* Required to make system bars fully transparent */
            //    getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            //            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            //            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
        //InputMethodManager sInputMethodManager = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        // DEBUG
    }

    @Override
    public void onPause() {
        super.onPause();
        CallBackHandler.sendCommand(CallBackHandler.CmdType.CMD_BLOCK);
        Log.e(TAG, "onPause");
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
                            onDestroy();
                        }
                    });
            infobox.show();
        }
    }

    private void parseNavigationURI(String schemeSpecificPart) {
        String[] naviData = schemeSpecificPart.split("&");
        Pattern p = Pattern.compile("(.*)=(.*)");
        Map<String, String> params = new HashMap<>();
        for (String naviDatum : naviData) {
            Matcher m = p.matcher(naviDatum);
            if (m.matches()) {
                params.put(m.group(1), m.group(2));
            }
        }
        // intent_data = "google.navigation:q=Wien Burggasse 27";
        // intent_data = "google.navigation:q=48.25676,16.643";
        // intent_data = "google.navigation:ll=48.25676,16.643&q=blabla-strasse";
        // intent_data = "google.navigation:ll=48.25676,16.643";
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
                        Message msg = Message.obtain(CallBackHandler.sCallbackHandler,
                                CallBackHandler.MsgType.CLB_SET_DESTINATION.ordinal());

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
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.activity_main_actions, menu);
        return super.onCreateOptionsMenu(menu);
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
        switch (item.getItemId()) {
            case R.id.optionsmenu_download_maps:
                Intent mapDownloadListActivity = new Intent(this, NavitDownloadSelectMapActivity.class);
                startActivityForResult(mapDownloadListActivity, Navit.NavitDownloaderSelectMap_id);
                break;
            case R.id.action_toggle_pois:
                // toggle the normal POI layers (to avoid double POIs)
                Message msg = Message.obtain(CallBackHandler.sCallbackHandler,
                        CallBackHandler.MsgType.CLB_CALL_CMD.ordinal());
                Bundle b = new Bundle();
                b.putString("cmd", "toggle_layer(\"POI Symbols\");");
                msg.setData(b);
                msg.sendToTarget();

                // toggle full POI icons on/off
                msg = Message.obtain(CallBackHandler.sCallbackHandler,
                        CallBackHandler.MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "toggle_layer(\"Android-POI-Icons-full\");");
                msg.setData(b);
                msg.sendToTarget();
                break;
            case R.id.optionsmenu_address_search:
                Intent searchIntent = new Intent(this, NavitAddressSearchActivity.class);
                this.startActivityForResult(searchIntent, NavitAddressSearch_id);
                break;
            case R.id.optionsmenu_set_maplocation:
                setMapLocation();
                break;
            case R.id.action_zoom_to_route:
                msg = Message.obtain(CallBackHandler.sCallbackHandler,
                        CallBackHandler.MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "zoom_to_route()");
                msg.setData(b);
                msg.sendToTarget();
                break;
            case R.id.action_stop_navigation:
                CallBackHandler.sendCommand(CallBackHandler.CmdType.CMD_CANCEL_ROUTE);
                break;
            case R.id.action_quit:
                this.onStop();
                this.onDestroy();
                break;
            case R.id.action_enable_auto_layout:
                msg = Message.obtain(CallBackHandler.sCallbackHandler, CallBackHandler.MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "switch_layout_day_night(\"auto\")");
                msg.setData(b);
                msg.sendToTarget();
                break;
            case R.id.action_toggle_layout:
                msg = Message.obtain(CallBackHandler.sCallbackHandler, CallBackHandler.MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "switch_layout_day_night(\"manual_toggle\")");
                msg.setData(b);
                msg.sendToTarget();
                break;
            case R.id.action_mapTileActivity:
                Intent intent = new Intent(this, MapsActivity.class);
                this.startActivityForResult(intent, MapsActivity_id);
                break;
            case R.id.optionsmenu_settings:
                setMapLocationNew();
                break;
            default:
                Log.w(TAG, "unhandled optionsItem");
        }
        //  Return false to allow normal menu processing to proceed, true to consume it here
        return false;
    }


    /**
     * Shows the native keyboard or other input method.
     *
     * @return 1 if keyboard is software, 0 if hardware
     */
    @SuppressWarnings("unused")
    int showNativeKeyboard() {
        Log.d(TAG, "showNativeKeyboard");
        Configuration config = getResources().getConfiguration();
        if ((config.keyboard == Configuration.KEYBOARD_QWERTY)
                && (config.hardKeyboardHidden == Configuration.HARDKEYBOARDHIDDEN_NO)) {
            /* physical keyboard present */
            return 0;
        }

        /* Use SHOW_FORCED here, else keyboard won't show in landscape mode */
        ((InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE))
                .showSoftInput(getCurrentFocus(), InputMethodManager.SHOW_FORCED);
        sShowSoftKeyboardShowing = true;

        return 1;
    }


    /**
     * Hides the native keyboard or other input method.
     */
    void hideNativeKeyboard() {
        ((InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE))
                .hideSoftInputFromWindow(getCurrentFocus().getWindowToken(), 0);
        sShowSoftKeyboardShowing = false;
    }


    void setDestination(float latitude, float longitude, String address) {
        Toast.makeText(getApplicationContext(), getTstring(R.string.address_search_set_destination) + "\n"
                + address, Toast.LENGTH_LONG).show(); //TRANS

        Bundle b = new Bundle();
        b.putFloat("lat", latitude);
        b.putFloat("lon", longitude);
        b.putString("q", address);
        Message msg = Message.obtain(CallBackHandler.sCallbackHandler,
                CallBackHandler.MsgType.CLB_SET_DESTINATION.ordinal());
        msg.setData(b);
        msg.sendToTarget();
    }

    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch (requestCode) {
            case Navit.NavitDownloaderSelectMap_id:
                if (resultCode == Activity.RESULT_OK) {
                    Message msg = mDialogs.obtainMessage(NavitDialogs.MSG_START_MAP_DOWNLOAD,
                            data.getIntExtra("map_index", -1), 0);
                    msg.sendToTarget();
                }
                break;
            case NavitAddressSearch_id:
                if (resultCode == Activity.RESULT_OK) {
                    Log.d(TAG, "received addressSearch OK");
                    Bundle destination = data.getExtras();
                    Toast.makeText(getApplicationContext(),
                            getTstring(R.string.address_search_set_destination) + "\n" + destination.getString(("q")),
                            Toast.LENGTH_LONG).show(); //TRANS

                    Message msg = Message.obtain(CallBackHandler.sCallbackHandler,
                            CallBackHandler.MsgType.CLB_SET_DESTINATION.ordinal());
                    msg.setData(destination);
                    msg.sendToTarget();
                }
                break;
            case NavitSelectStorage_id:
                if (resultCode == RESULT_OK) {
                    String newDir = data.getStringExtra(FileBrowserActivity.returnDirectoryParameter);
                    Log.d(TAG, "selected path= " + newDir);
                    if (!newDir.contains("/navit")) {
                        newDir = newDir + "/navit";
                    }
                    SharedPreferences prefs = this.getSharedPreferences(NavitAppConfig.NAVIT_PREFS, MODE_PRIVATE);
                    SharedPreferences.Editor prefsEditor = prefs.edit();
                    prefsEditor.putString("filenamePath", newDir);
                    prefsEditor.apply();

                    Toast.makeText(this, String.format(getTstring(R.string.map_location_changed), newDir),
                            Toast.LENGTH_LONG).show();
                } else {
                    Log.w(TAG, "select path failed");
                }
                break;
            case MapsActivity_id:
                break;
            default:
                if (mActivityResults[requestCode] != null) { // this is used to set up the TTS engine
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
        Intent fileExploreIntent = new Intent(this, FileBrowserActivity.class);
        fileExploreIntent
                .putExtra(FileBrowserActivity.startDirectoryParameter, "/mnt")
                .setAction(FileBrowserActivity.INTENT_ACTION_SELECT_DIR);
        startActivityForResult(fileExploreIntent, NavitSelectStorage_id);
    }

    private void setMapLocationNew() {
        Intent fileExploreIntent = new Intent(this, NavitSettingsActivity.class);
        fileExploreIntent.setAction(FileBrowserActivity.INTENT_ACTION_SELECT_DIR);
        startActivityForResult(fileExploreIntent, NavitSelectStorage_id);
    }


    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy");
        NavitVehicle.removeListeners();
        navitDestroy();
    }


    public void onStop() {
        super.onStop();
        Log.d(TAG, "onStop");
    }

    void showActionBar(boolean show) {
        if (this.getSupportActionBar() == null) {
            return;
        }
        if (show) {
            this.getSupportActionBar().show();
        } else {
            this.getSupportActionBar().hide();
        }
    }

    void fullscreen(int fullscreen) {
        int width;
        int height;
        View decorView = getWindow().getDecorView(); //?????

        mIsFullscreen = (fullscreen != 0);
        if (mIsFullscreen) {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            int uiOptions = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
            decorView.setSystemUiVisibility(uiOptions);

        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
            decorView.setSystemUiVisibility(0);
        }
    }

    public void disableSuspend() {
        mWakeLock.acquire();
        mWakeLock.release();
    }

    private native void navitMain(String lang, String path, String path2);

    public native void navitDestroy();

}
