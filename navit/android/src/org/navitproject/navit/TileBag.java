package org.navitproject.navit;


import android.util.Log;

import com.google.android.gms.maps.model.Tile;

import java.util.HashMap;


class TileBag extends HashMap<String,TileXYZP> {


    TileBag(){
        super();
    }


    public TileXYZP put(String string, TileXYZP tileXYZP) {
        super.put(string, tileXYZP);
        if (tileXYZP.getTile() != null) {
            tileXYZP.pending.release();
        }
        return tileXYZP;
    }


    Tile getTile(int x,int y,int z) {

        if (this.get("" + x + "," + y + "," + z ) == null){
            Log.e("AZERT", "no tileXYZP in hash" );
            return null;
        }

        this.get("" + x + "," + y + "," + z ).acquire();
        if (this.get("" + x + "," + y + "," + z ).getTile() == null){
            Log.e("AZERT", "tileXYZP in hash has no tile" );
            return null;
        }

        // googlemap has a cache of its own
        return this.remove("" + x + "," + y + "," + z ).getTile();
    }

}
