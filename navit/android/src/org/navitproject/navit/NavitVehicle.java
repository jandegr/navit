/**
 * Navit, a modular navigation system. Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if
 * not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

package org.navitproject.navit;

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import java.util.List;

public class NavitVehicle {

    private static final String GPS_FIX_CHANGE = "android.location.GPS_FIX_CHANGE";
    private static final String TAG = NavitVehicle.class.getName();
    public static Location lastLocation = null;
    private static LocationManager sLocationManager = null;
    private static NavitLocationListener preciseLocationListener = null;
    private static NavitLocationListener fastLocationListener = null;
    private int vehicle_pcbid;
    private int vehicle_fcbid;
    private String fastProvider;

    /**
     * Creates a new {@code NavitVehicle}
     *
     * @param pcbid The address of the position callback function which will be called when a
     * location update is received
     * @param scbid The address of the status callback function which will be called when a status
     * update is received
     * @param fcbid The address of the fix callback function which will be called when a {@code
     * android.location.GPS_FIX_CHANGE} is received, indicating a change in GPS fix status
     */
    NavitVehicle(Context navit, int pcbid, int scbid, int fcbid) {
        if (ContextCompat.checkSelfPermission(navit, Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {
            // Permission is not granted
            return;
        }
        sLocationManager = (LocationManager) navit.getSystemService(Context.LOCATION_SERVICE);
        preciseLocationListener = new NavitLocationListener();
        preciseLocationListener.precise = true;
        fastLocationListener = new NavitLocationListener();

		/* Use 2 LocationProviders, one precise (usually GPS), and one
		   not so precise, but possible faster. The fast provider is
		   disabled when the precise provider gets its first fix. */

        // Selection criteria for the precise provider
        Criteria highCriteria = new Criteria();
        highCriteria.setAccuracy(Criteria.ACCURACY_FINE);
        highCriteria.setAltitudeRequired(true); //we gebruiken de hoogte toch niet ??
        highCriteria.setBearingRequired(true);
        highCriteria.setCostAllowed(true);
        highCriteria.setPowerRequirement(Criteria.POWER_HIGH);

        // Selection criteria for the fast provider
        Criteria lowCriteria = new Criteria();
        lowCriteria.setAccuracy(Criteria.ACCURACY_COARSE);
        lowCriteria.setAltitudeRequired(false);
        lowCriteria.setBearingRequired(false);
        lowCriteria.setCostAllowed(true);
        lowCriteria.setPowerRequirement(Criteria.POWER_HIGH);

        Log.d(TAG, "Providers " + sLocationManager.getAllProviders());

        String preciseProvider = sLocationManager.getBestProvider(highCriteria, false);
        Log.d(TAG, "Precise Provider " + preciseProvider);
        fastProvider = sLocationManager.getBestProvider(lowCriteria, false);

        vehicle_pcbid = pcbid;
        vehicle_fcbid = fcbid;

        navit.registerReceiver(preciseLocationListener, new IntentFilter(GPS_FIX_CHANGE));
        sLocationManager.requestLocationUpdates(preciseProvider, 0, 0, preciseLocationListener);

        if (fastProvider == null || preciseProvider.compareTo(fastProvider) == 0) {
            List<String> fastProviderList = sLocationManager.getProviders(lowCriteria, false);
            fastProvider = null;
            for (String fastCandidate : fastProviderList) {
                if (preciseProvider.compareTo(fastCandidate) != 0) {
                    fastProvider = fastCandidate;
                    Log.d(TAG, "Fast Provider changed to " + fastProvider);
                    break;
                }
            }
        }
        if (fastProvider != null) {
            sLocationManager.requestLocationUpdates(fastProvider, 0, 0, fastLocationListener);
        }
    }

    public static void removeListener(Navit navit) {
        Log.d(TAG, "removing locationlisteners");
        if (sLocationManager != null) {
            if (preciseLocationListener != null) {
                sLocationManager.removeUpdates(preciseLocationListener);
                navit.unregisterReceiver(preciseLocationListener);
            }
            if (fastLocationListener != null) {
                sLocationManager.removeUpdates(fastLocationListener);
            }
        }
    }

    @SuppressWarnings("JniMissingFunction")
    public native void VehicleCallback(int id, Location location);

    @SuppressWarnings("JniMissingFunction")
    public native void VehicleCallback(int id, int enabled);

    private class NavitLocationListener extends BroadcastReceiver implements LocationListener {

        boolean precise = false;

        public void onLocationChanged(Location location) {
            lastLocation = location;
            // Disable the fast provider if still active
            if (precise && fastProvider != null) {
                sLocationManager.removeUpdates(fastLocationListener);
                fastProvider = null;
            }
            VehicleCallback(vehicle_pcbid, location);
            VehicleCallback(vehicle_fcbid, 1);
        }

        public void onProviderDisabled(String provider) {
        }

        public void onProviderEnabled(String provider) {
        }

        public void onStatusChanged(String provider, int status, Bundle extras) {
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction() != null && intent.getAction().equals(GPS_FIX_CHANGE)) {
                if (intent.getBooleanExtra("enabled", false)) {
                    VehicleCallback(vehicle_fcbid, 1);
                } else if (!intent.getBooleanExtra("enabled", true)) {
                    VehicleCallback(vehicle_fcbid, 0);
                }
            }
        }
    }
}
