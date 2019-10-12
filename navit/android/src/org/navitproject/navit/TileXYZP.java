package org.navitproject.navit;

import android.graphics.Bitmap;

import com.google.android.gms.maps.model.Tile;

import java.util.concurrent.Semaphore;



public class TileXYZP {


    final Semaphore pending = new Semaphore(0);
    Bitmap mBitmap = null;
    private final int x;
    private final int y;
    private final int z;
    private Tile tile;

    TileXYZP(int x, int y, int z, Tile tile, Bitmap bitmap){
        this.x = x;
        this.y = y;
        this.z = z;
        this.tile = tile;
        this.mBitmap = bitmap;
    }

    void setTile (Tile tile) {
        this.tile = tile;
        pending.release();
    }


    void acquire() {
        try {
            pending.acquire();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public String toString(){
        return ("" + x + "," + y + "," + z );
    }

    Tile getTile (){
        return this.tile;
    }

    public int getSize() {
        return 256; ///////////// hack, must not be hardcoded !!!!
    }
}
