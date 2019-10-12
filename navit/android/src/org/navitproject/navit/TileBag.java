package org.navitproject.navit;

import android.util.Log;

import com.google.android.gms.maps.model.Tile;

import java.util.HashMap;


class TileBag extends HashMap<String, TileXYZp> {


    TileBag() {
        super();
    }


    public TileXYZp put(String string, TileXYZp tileP) {
        super.put(string, tileP);
        if (tileP.getTile() != null) {
            tileP.mPending.release();
        }
        return tileP;
    }


    Tile getTile(int x,int y,int z) {

        if (this.get("" + x + "," + y + "," + z) == null) {
            Log.e("AZERT", "no tileXYZP in hash");
            return null;
        }

        this.get("" + x + "," + y + "," + z).acquire();
        if (this.get("" + x + "," + y + "," + z).getTile() == null) {
            Log.e("AZERT", "tileXYZP in hash has no tile");
            return null;
        }

        // googlemap has a cache of its own
        return this.remove("" + x + "," + y + "," + z).getTile();
    }

}
