package org.navitproject.navit;

import android.util.Log;

import com.google.android.gms.maps.model.Tile;

import java.util.HashMap;


class TileBag extends HashMap<String, TileXYZp> {
    private static final  String TAG = "TileBag";


    TileBag() {

        super();
        Log.e(TAG, "init");
    }


    public TileXYZp put(String string, TileXYZp tileP) {
        super.put(string, tileP);
        Log.e(TAG, "put done");
        return tileP;
    }


    Tile getTile(int x, int y, int z) {

        if (this.get("" + x + "," + y + "," + z) == null) {
            Log.e(TAG, "no tileXYZP in hash");
            return null;
        }

        this.get("" + x + "," + y + "," + z).acquire();
        if (this.get("" + x + "," + y + "," + z).getTile() == null) {
            Log.e(TAG, "tileXYZP in hash has no tile");
            return null;
        }

        // googlemap has a cache of its own
        return this.remove("" + x + "," + y + "," + z).getTile();
    }

}
