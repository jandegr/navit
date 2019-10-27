package org.navitproject.navit;

import android.graphics.Bitmap;
import android.util.Log;

import com.google.android.gms.maps.model.Tile;

import java.util.concurrent.Semaphore;



public class TileXYZp {


    private final Semaphore mPending = new Semaphore(0);
    Bitmap mBitmap;
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
        mPending.release();
    }


    void acquire() {
        try {
            mPending.acquire();
        } catch (InterruptedException e) {
            e.printStackTrace();
            Log.e("acquire ","interrupted");
        }
    }

    public String toString() {
        return ("" + mX + "," + mY + "," + mZ);
    }

    Tile getTile() {
        //Log.e("getTile"," data length = " + mTile.data.length);
        return mTile;
    }

    public int getSize() {
        return 256; ///////////// hack, must not be hardcoded !!!!
    }
}
