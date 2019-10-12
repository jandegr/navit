package org.navitproject.navit;

import android.graphics.Bitmap;

import com.google.android.gms.maps.model.Tile;

import java.util.concurrent.Semaphore;



public class TileXYZp {


    final Semaphore pending = new Semaphore(0);
    Bitmap mBitmap = null;
    private final int mX;
    private final int mY;
    private final int mZ;
    private Tile mTile;

    TileXYZp(int x, int y, int z, Tile tile, Bitmap bitmap) {
        this.mX = x;
        this.mY = y;
        this.mZ = z;
        this.mTile = tile;
        this.mBitmap = bitmap;
    }

    void setTile(Tile tile) {
        this.mTile = tile;
        pending.release();
    }


    void acquire() {
        try {
            pending.acquire();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public String toString() {
        return ("" + mX + "," + mY + "," + mZ);
    }

    Tile getTile() {
        return this.mTile;
    }

    public int getSize() {
        return 256; ///////////// hack, must not be hardcoded !!!!
    }
}
