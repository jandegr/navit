/**
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

import android.os.Handler;
import android.os.Message;
import android.util.Log;



class NavitTimeout implements Runnable {
    private static Handler handler = new Handler() {
            public void handleMessage(Message m) {
                Log.e("Navit","Handler received message");
            }
        };
    private boolean eventMulti;
    private int eventCallbackid;
    private int eventTimeout;

    NavitTimeout(int timeout, boolean multi, int callbackid) {
        eventTimeout = timeout;
        eventMulti = multi;
        eventCallbackid = callbackid;
        handler.postDelayed(this, eventTimeout);
    }

    public native void timeoutCallback(int id);

    public void run() {
        // Log.e("Navit","Handle Event");
        if (eventMulti) {
            handler.postDelayed(this, eventTimeout);
        }
        timeoutCallback(eventCallbackid);
    }

    public void remove() {
        handler.removeCallbacks(this);
    }
}

