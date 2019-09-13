/**
 * Navit, a modular navigation system. Copyright (C) 2005-2008 Navit Team <p> This program is free software; you can
 * redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation. <p> This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details. <p> You should have received a copy of the GNU General Public License along with this
 * program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
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
import android.content.res.Resources;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Message;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.util.Linkify;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.RelativeLayout;
import android.widget.TextView;
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
import org.navitproject.navit.NavitGraphics.MsgType;

public class Navit extends Activity {

    static final String NAVIT_PREFS = "NavitPrefs";
    private static final int MY_PERMISSIONS_REQUEST_ALL = 101;
    private static final int MY_PERMISSIONS_STORAGE_REQUEST = 102;
    private static final int MY_PERMISSIONS_FINE_LOCATION_REQUEST = 103;
    private static final int NavitDownloaderSelectMap_id = 967;
    private static final int NavitAddressSearch_id = 70;
    private static final int NavitSelectStorage_id = 43;
    private static final int NavitSettings_id = 80;
    private static final String NAVIT_PACKAGE_NAME = "org.navitproject.navit";
    private static final String TAG = "Navit";
    public static InputMethodManager mgr = null;
    public static DisplayMetrics metrics = null;
    public static Boolean show_soft_keyboard = false;
    public static Boolean show_soft_keyboard_now_showing = false;
    public static long last_pressed_menu_key = 0L;
    public static long time_pressed_menu_key = 0L;
    private static Resources NavitResources = null;
    // define callback id here
    public static NavitGraphics mNavitGraphics = null;
    static String mapFilenamePath = null;
    private static Intent startupIntent = null;
    private static long startupIntentTimestamp = 0L;
    private static Navit navit;
    private NavitDialogs mDialogs;
    private NavitActivityResult[] mActivityResults;

    static Navit getInstance() {
        return navit;
    }


    // callback id gets set here when called from NavitGraphics
    public void setKeypressCallback(long kpCbId, NavitGraphics ng) {
        mNavitGraphics = ng;
    }

    public void setMotionCallback(long moCbId, NavitGraphics navitGraphics) {
        this.mNavitGraphics = navitGraphics;
    }

    static native void navitMain(Navit navit, String lang, int version,
            String displaydensity, String path, String mappath);

    /*
     * this is used to load the 'navit' native library on
     * application startup. The library has already been unpacked at
     * installation time by the package manager.
     */
    private static void loadNativeNavit() {
        System.loadLibrary("navit");
    }

    // not used !!!
    void removeFileIfExists(String source) {
        File file = new File(source);
        if (!file.exists()) {
            return;
        }
        file.delete();
    }

    // not used !!!
    void copyFileIfExists(String source, String destination) throws IOException {
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

    @SuppressWarnings("BooleanMethodIsAlwaysInverted")
    private boolean extractRes(String resname, String result) {
        boolean needsUpdate = false;
        Log.e(TAG, "Res Name " + resname + ", result " + result);
        int id = NavitResources.getIdentifier(resname, "raw", NAVIT_PACKAGE_NAME);
        Log.e(TAG, "Res ID " + id);
        if (id == 0) {
            return false;
        }
        File resultfile = new File(result);
        if (!resultfile.exists()) {
            needsUpdate = true;
            File path = resultfile.getParentFile();
            if (!path.exists() && !resultfile.getParentFile().mkdirs()) {
                return false;
            }
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
            if (apkUpdateTime > resultfile.lastModified()) {
                needsUpdate = true;
            }
        }

        if (needsUpdate) {
            Log.d(TAG, "Extracting resource");
            try {
                InputStream resourcestream = NavitResources.openRawResource(id);
                FileOutputStream resultfilestream = new FileOutputStream(resultfile);
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
        SharedPreferences settings = getSharedPreferences(NAVIT_PREFS, MODE_PRIVATE);
        boolean firstStart = settings.getBoolean("firstStart", true);

        if (firstStart) {
            AlertDialog.Builder infobox = new AlertDialog.Builder(this);
            infobox.setTitle(getTstring(R.string.initial_info_box_title)); // TRANS
            infobox.setCancelable(false);
            final TextView message = new TextView(this);
            message.setFadingEdgeLength(20);
            message.setVerticalFadingEdgeEnabled(true);
            // message.setVerticalScrollBarEnabled(true);
            RelativeLayout.LayoutParams rlp = new RelativeLayout.LayoutParams(
                    RelativeLayout.LayoutParams.FILL_PARENT,
                    RelativeLayout.LayoutParams.FILL_PARENT);
            message.setLayoutParams(rlp);
            final SpannableString s = new SpannableString(
                    (getTstring(R.string.initial_info_box_message))); // TRANS
            Linkify.addLinks(s, Linkify.WEB_URLS);
            message.setText(s);
            message.setMovementMethod(LinkMovementMethod.getInstance());
            infobox.setView(message);
            // TRANS
            infobox.setPositiveButton(getTstring(R.string.initial_info_box_OK),
                    new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface arg0, int arg1) {
                            Log.d(TAG, "Ok, user saw the infobox");
                        }
                    });
            // TRANS
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
            SharedPreferences.Editor editSettings = settings.edit();
            editSettings.putBoolean("firstStart", false);
            editSettings.apply();
        }
    }

    /**
     * Called when the activity is first created.
     */

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        navit = this;
        mDialogs = new NavitDialogs(this);
        NavitResources = getResources();
        // only take arguments here, onResume gets called all the time (e.g. when screenblanks, etc.)
        Navit.startupIntent = this.getIntent();
        // hack! Remember time stamps, and only allow 4 secs. later in onResume to set target!
        Navit.startupIntentTimestamp = System.currentTimeMillis();
        Log.e(TAG, "**1**A " + startupIntent.getAction());
        Log.e(TAG, "**1**D " + startupIntent.getDataString());
        Log.i(TAG, "arch = " + System.getProperty("os.arch"));
        Log.i(TAG, "android.os.Build.VERSION.SDK_INT = " + android.os.Build.VERSION.SDK_INT);
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG, "ask for permission storage");
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    MY_PERMISSIONS_STORAGE_REQUEST);
        }

        if ((ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED)) {
            Log.d(TAG, "ask for permission location");
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.ACCESS_FINE_LOCATION},
                    MY_PERMISSIONS_FINE_LOCATION_REQUEST);
        }

        //  if ((ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
        // != PackageManager.PERMISSION_GRANTED) &&
        //          (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
        // != PackageManager.PERMISSION_GRANTED)){
        //      ActivityCompat.requestPermissions(this,
        //              new String[]{Manifest.permission.ACCESS_FINE_LOCATION},
        //              MY_PERMISSIONS_REQUEST_ALL);
        //  }

        //  if ((ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
        // == PackageManager.PERMISSION_GRANTED) &&
        //          (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
        // == PackageManager.PERMISSION_GRANTED)){
        //      runNavit();
        //  }
        //}

        loadNativeNavit();

        Locale locale = java.util.Locale.getDefault();
        String lang = locale.getLanguage();
        String langc = lang;
        Log.e("Navit", "lang=" + lang);
        int pos = lang.indexOf('_');
        String navitLanguage;
        if (pos != -1) {
            langc = lang.substring(0, pos);
            navitLanguage = langc + lang.substring(pos).toUpperCase(locale);
            Log.e("Navit", "substring lang " + navitLanguage.substring(pos).toUpperCase(locale));
        } else {
            String country = locale.getCountry();
            Log.e("Navit", "Country1 " + country);
            Log.e("Navit", "Country2 " + country.toUpperCase(locale));
            navitLanguage = langc + "_" + country.toUpperCase(locale);
        }
        Log.i(TAG, "Language " + lang);
        SharedPreferences prefs = getSharedPreferences(NAVIT_PREFS, MODE_PRIVATE);
        mapFilenamePath = prefs.getString("filenamePath",
                Environment.getExternalStorageDirectory().getPath() + "/navit/");

        //map_filename_path  = prefs.getString("filenamePath", Environment.getExternalStorageDirectory().
        // getPath() + "/navit/");

        Log.e(TAG, "getExternalStorageDirectory " + Environment.getExternalStorageDirectory());

        File[] navitfiles = ContextCompat.getExternalFilesDirs(this, null);
        for (File f : navitfiles) {
            Log.e(TAG, "compat file " + f);
        }

        Log.e(TAG,"package = " + getApplicationContext().getPackageName());


        //map_filename_path = prefs.getString("filenamePath", navitfiles[0].toString() + "/");
        mapFilenamePath  = prefs.getString("filenamePath", getApplicationContext().getFilesDir().getPath()+'/');
        Log.e(TAG,"path = " + mapFilenamePath);
        File navitMapsDir = new File(mapFilenamePath);
        //map_filename_path = navitfiles[0].getParent() + "/";

        navitMapsDir.mkdirs();
        final String navitDataDir = getFilesDir().toString();
        //final String NAVIT_DATA_DIR = navitfiles[0].getParent();
        final String navitDataShare = navitDataDir + "/share";

        Log.e(TAG,"data dir = " + navitDataDir);
        Log.e(TAG,"data share dir = " + navitDataShare);
        // make sure the share dir exists
        File navitDataShareDir = new File(navitDataShare);
        navitDataShareDir.mkdirs();

        Display display = getWindowManager().getDefaultDisplay();
        metrics = new DisplayMetrics();
        display.getMetrics(Navit.metrics);
        int densityDpi = (int) ((Navit.metrics.density * 160) - .5f);

        Log.i(TAG, "Navit -> dpi=" + densityDpi);
        Log.i(TAG, "Navit -> density=" + Navit.metrics.density);
        Log.i(TAG, "Navit -> scaledDensity=" + Navit.metrics.scaledDensity);

        mActivityResults = new NavitActivityResult[16];
        setVolumeControlStream(AudioManager.STREAM_MUSIC);

        Window w = this.getWindow();
        w.setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

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
        if (!extractRes("navit" + myDisplayDensity, navitDataDir + "/share/navit.xml")) {
            Log.e(TAG, "Failed to extract navit.xml for " + myDisplayDensity);
        }

        navitMain(this, navitLanguage, android.os.Build.VERSION.SDK_INT, myDisplayDensity,
                navitDataDir + "/bin/navit", mapFilenamePath);
        showInfos();
        Navit.mgr = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

    }

    protected void onRestart() {
        Log.w(TAG, "onRestart");
        super.onRestart();
    }


    @Override
    protected void onResume() {
        super.onResume();
        Log.d(TAG, "OnResume");
        //InputMethodManager mgr = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        // DEBUG
        // intent_data = "google.navigation:q=Wien Burggasse 27";
        // intent_data = "google.navigation:q=48.25676,16.643";
        // intent_data = "google.navigation:ll=48.25676,16.643&q=blabla-strasse";
        // intent_data = "google.navigation:ll=48.25676,16.643";

        if (startupIntent != null) {
            if (System.currentTimeMillis() <= Navit.startupIntentTimestamp + 4000L) {
                Log.i(TAG, "**2**A " + startupIntent.getAction());
                Log.i(TAG, "**2**D " + startupIntent.getDataString());
                String naviScheme = startupIntent.getScheme();
                if (naviScheme != null && naviScheme.equals("google.navigation")) {
                    parseNavigationURI(startupIntent.getData().getSchemeSpecificPart());
                }
            } else {
                Log.w(TAG, "timestamp for navigate_to expired! not using data");
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
            int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        Log.e(TAG, grantResults.length + "permissions");

        switch (requestCode) {
            case MY_PERMISSIONS_REQUEST_ALL: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 1 && grantResults[0] == PackageManager.PERMISSION_GRANTED
                        && grantResults[1] == PackageManager.PERMISSION_GRANTED) {
                    // ok, we got permissions
                    //                  permissionsOK.notify();
                } else {
                    AlertDialog.Builder infobox = new AlertDialog.Builder(this);
                    infobox.setTitle(getString(R.string.permissions_info_box_title)); // TRANS
                    infobox.setCancelable(false);
                    final TextView message = new TextView(this);
                    message.setFadingEdgeLength(20);
                    message.setVerticalFadingEdgeEnabled(true);
                    RelativeLayout.LayoutParams rlp = new RelativeLayout.LayoutParams(
                            RelativeLayout.LayoutParams.FILL_PARENT,
                            RelativeLayout.LayoutParams.FILL_PARENT);
                    message.setLayoutParams(rlp);
                    final SpannableString s = new SpannableString(
                            getString(R.string.permissions_not_granted)); // TRANS
                    message.setText(s);
                    message.setMovementMethod(LinkMovementMethod.getInstance());
                    infobox.setView(message);
                    // TRANS
                    infobox.setPositiveButton(getString(R.string.initial_info_box_OK),
                            new DialogInterface.OnClickListener() {
                                public void onClick(DialogInterface arg0, int arg1) {
                                    exit();
                                }
                            });
                    infobox.show();
                }
            }
            break;
            case MY_PERMISSIONS_STORAGE_REQUEST: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    // ok, we got permissions
                } else {
                    AlertDialog.Builder infobox = new AlertDialog.Builder(this);
                    infobox.setTitle(getString(R.string.permissions_info_box_title)); // TRANS
                    infobox.setCancelable(false);
                    final TextView message = new TextView(this);
                    message.setFadingEdgeLength(20);
                    message.setVerticalFadingEdgeEnabled(true);
                    RelativeLayout.LayoutParams rlp = new RelativeLayout.LayoutParams(
                            RelativeLayout.LayoutParams.FILL_PARENT,
                            RelativeLayout.LayoutParams.FILL_PARENT);
                    message.setLayoutParams(rlp);
                    final SpannableString s = new SpannableString(
                            getString(R.string.permissions_not_granted)); // TRANS
                    message.setText(s);
                    message.setMovementMethod(LinkMovementMethod.getInstance());
                    infobox.setView(message);
                    // TRANS
                    infobox.setPositiveButton(getString(R.string.initial_info_box_OK),
                            new DialogInterface.OnClickListener() {
                                public void onClick(DialogInterface arg0, int arg1) {
                                    exit();
                                }
                            });
                    infobox.show();
                }
            }
            break;
            case MY_PERMISSIONS_FINE_LOCATION_REQUEST: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    // ok, we got permissions
                    //permissionsOK.release();
                } else {
                    AlertDialog.Builder infobox = new AlertDialog.Builder(this);
                    infobox.setTitle(getString(R.string.permissions_info_box_title)); // TRANS
                    infobox.setCancelable(false);
                    final TextView message = new TextView(this);
                    message.setFadingEdgeLength(20);
                    message.setVerticalFadingEdgeEnabled(true);
                    RelativeLayout.LayoutParams rlp = new RelativeLayout.LayoutParams(
                            RelativeLayout.LayoutParams.FILL_PARENT,
                            RelativeLayout.LayoutParams.FILL_PARENT);
                    message.setLayoutParams(rlp);
                    final SpannableString s = new SpannableString(
                            getString(R.string.permissions_not_granted)); // TRANS
                    message.setText(s);
                    message.setMovementMethod(LinkMovementMethod.getInstance());
                    infobox.setView(message);
                    // TRANS
                    infobox.setPositiveButton(getString(R.string.initial_info_box_OK),
                            new DialogInterface.OnClickListener() {
                                public void onClick(DialogInterface arg0, int arg1) {
                                    exit();
                                }
                            });
                    infobox.show();
                }
            }
            break;
            default:
                break;
        }
        //if ((ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
        //        != PackageManager.PERMISSION_GRANTED)
        //        && (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
        //                != PackageManager.PERMISSION_GRANTED)) {
        //}
    }

    private void parseNavigationURI(String schemeSpecificPart) {
        String[] naviData = schemeSpecificPart.split("&");
        Pattern p = Pattern.compile("(.*)=(.*)");
        Map<String, String> params = new HashMap<>();
        for (String aNaviData : naviData) {
            Matcher m = p.matcher(aNaviData);
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
                        Message msg = Message.obtain(NavitGraphics.callbackHandler,
                                MsgType.CLB_SET_DESTINATION.ordinal());
                        msg.setData(b);
                        msg.sendToTarget();
                        Log.d(TAG, "target found (b): " + geoString);
                    } catch (NumberFormatException e) {
                        e.printStackTrace();
                    } // nothing to do here
                }
            } else {
                start_targetsearch_from_intent(geoString);
            }
        }
    }

    public void setActivityResult(int requestCode, NavitActivityResult activityResult) {
        //Log.e("Navit", "setActivityResult " + requestCode);
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
                    Toast.LENGTH_LONG).show();
        } else {
            Intent searchIntent = new Intent(this, NavitAddressSearchActivity.class);
            searchIntent.putExtra("search_string", targetAddress);
            this.startActivityForResult(searchIntent, NavitAddressSearch_id);
        }
    }

    @SuppressWarnings("EmptyMethod")
    public void runOptionsItem(int id) {
        // dummy voor de native code
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case 1:
                // zoom in
                Message.obtain(mNavitGraphics.callbackHandler,
                        MsgType.CLB_ZOOM_IN.ordinal()).sendToTarget();
                Log.i("Navit", "onOptionsItemSelected -> zoom in");
                break;
            case 2:
                // zoom out
                Message.obtain(mNavitGraphics.callbackHandler,
                        MsgType.CLB_ZOOM_OUT.ordinal()).sendToTarget();
                Log.i("Navit", "onOptionsItemSelected -> zoom out");
                break;
            case R.id.optionsmenu_download_maps:
                Intent mapDownloadListActivity = new Intent(this,
                        NavitDownloadSelectMapActivity.class);
                startActivityForResult(mapDownloadListActivity,
                        Navit.NavitDownloaderSelectMap_id);
                break;
            case 5:
                // toggle the normal POI layers (to avoid double POIs)
                Message msg = Message.obtain(mNavitGraphics.callbackHandler,
                        MsgType.CLB_CALL_CMD.ordinal());
                Bundle b = new Bundle();
                b.putString("cmd", "toggle_layer(\"POI Symbols\");");
                msg.setData(b);
                msg.sendToTarget();

                // toggle full POI icons on/off
                msg = Message.obtain(mNavitGraphics.callbackHandler,
                        MsgType.CLB_CALL_CMD.ordinal());
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
                msg = Message.obtain(mNavitGraphics.callbackHandler,
                        MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "zoom_to_route()");
                msg.setData(b);
                msg.sendToTarget();
                break;
            case R.id.toggle_autozoom:
                msg = Message.obtain(mNavitGraphics.callbackHandler,
                        MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "autozoom_active=autozoom_active==0?1:0");
                msg.setData(b);
                msg.sendToTarget();
                break;
            case R.id.action_stop_navigation:
                msg = Message.obtain(mNavitGraphics.callbackHandler,
                        MsgType.CLB_ABORT_NAVIGATION.ordinal());
                msg.sendToTarget();
                break;
            case R.id.optionsmenu_settings:
                Intent i = new Intent(this, NavitSettingsActivity.class);
                startActivityForResult(i, NavitSettings_id);
                break;
            case R.id.action_quit:
                this.onStop();
                this.onDestroy();
                break;
            case R.id.action_enable_auto_layout:
                msg = Message.obtain(mNavitGraphics.callbackHandler,
                        MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "switch_layout_day_night(\"auto\")");
                msg.setData(b);
                msg.sendToTarget();
                break;
            case R.id.action_toggle_layout:
                msg = Message.obtain(mNavitGraphics.callbackHandler,
                        MsgType.CLB_CALL_CMD.ordinal());
                b = new Bundle();
                b.putString("cmd", "switch_layout_day_night(\"manual_toggle\")");
                msg.setData(b);
                msg.sendToTarget();
                break;
            default:
                break;
        }
        //  Return false to allow normal menu processing to proceed, true to consume it here
        return false;
    }

    void setDestination(float latitude, float longitude, String address) {
        Toast.makeText(getApplicationContext(),
                getTstring(R.string.address_search_set_destination) + "\n" + address,
                Toast.LENGTH_LONG).show();
        Bundle b = new Bundle();
        b.putFloat("lat", latitude);
        b.putFloat("lon", longitude);
        b.putString("q", address);
        Message msg = Message.obtain(mNavitGraphics.callbackHandler,
                MsgType.CLB_SET_DESTINATION.ordinal());
        msg.setData(b);
        msg.sendToTarget();
    }

    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
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
                    Bundle destination = data.getExtras();
                    Toast.makeText(getApplicationContext(),
                            getTstring(R.string.address_search_set_destination) + "\n" + destination
                                    .getString(("q")), Toast.LENGTH_LONG).show(); //TRANS
                    Message msg = Message.obtain(mNavitGraphics.callbackHandler,
                            MsgType.CLB_SET_DESTINATION.ordinal());
                    msg.setData(destination);
                    msg.sendToTarget();
                }
                break;
            case NavitSelectStorage_id:
                if (resultCode == RESULT_OK) {
                    String newDir = data
                            .getStringExtra(FileBrowserActivity.returnDirectoryParameter);
                    Log.d(TAG, "selected path= " + newDir);
                    SharedPreferences prefs = this.getSharedPreferences(NAVIT_PREFS, MODE_PRIVATE);
                    SharedPreferences.Editor prefsEditor = prefs.edit();
                    prefsEditor.putString("filenamePath", newDir + "/navit/");
                    prefsEditor.apply();
                    mapFilenamePath = newDir + "/navit/";
                    Toast.makeText(this, "New location set to " + mapFilenamePath
                            + "\n Restart Navit to apply the changes", Toast.LENGTH_LONG).show();
                } else {
                    Log.w(TAG, "select path failed");
                }
                break;
            case NavitSettings_id:
                break;
            default:
                //Log.e("Navit", "onActivityResult " + requestCode + " " + resultCode);
                mActivityResults[requestCode].onActivityResult(requestCode, resultCode, data);
                break;
        }
    }

    @Override
    protected void onPrepareDialog(int id, Dialog dialog) {
        mDialogs.prepareDialog(id, dialog);
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
        fileExploreIntent.putExtra(FileBrowserActivity.startDirectoryParameter, "/mnt")
                .setAction(FileBrowserActivity.INTENT_ACTION_SELECT_DIR);
        startActivityForResult(fileExploreIntent, NavitSelectStorage_id);
    }

    protected void onPause() {
        Log.w("Navit", "onPause");
        super.onPause();
    }

    protected void onStop() {
        Log.w("Navit", "onStop");
        if (isFinishing()) {
            Log.w("Navit", "onStop Finishing");
        }
        super.onStop();
    }

    //leverde nullpointer
    //wordt bljkbaar vanuit native aangeroepen

    @Override
    protected void onDestroy() {
        NavitVehicle.removeListener(this);
        Log.w("Navit", "OnDestroy");

        if (isFinishing()) {
            Log.w("Navit", "onDestroy Finishing");
        }
        super.onDestroy();
        Log.w("Navit", "OnDestroy2");
        navitDestroy();
    }

    @SuppressWarnings("unused")
    void fullscreen(int fullscreen) {
        if (fullscreen != 0) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
            if (this.getActionBar() != null) {
                this.getActionBar().hide();
            }
        } else {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            if (this.getActionBar() != null) {
                this.getActionBar().show();
            }
        }
    }

    @SuppressWarnings("EmptyMethod")
    public void disableSuspend() {
        //  wakelock.acquire();
        //  wakelock.release();
    }

    private void exit() {
        Log.w("Navit", "exit--");
        NavitVehicle.removeListener(this);
        super.onDestroy();
        navitDestroy();
    }

    native void navitDestroy();
}





















