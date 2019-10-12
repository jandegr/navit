package org.navitproject.navit;

import android.graphics.Bitmap;
import android.util.Log;

import com.google.android.gms.maps.model.Tile;
import com.google.android.gms.maps.model.TileProvider;

class NavitMapTileProvider implements TileProvider {

    private static final String TAG = "MapTileProvider";

    private native int getTileOpenGL(int size, int z, int x, int y);

    private final int mSize;

    private boolean mNativeIsDrawing = false;

    private final TileCompressor mTileCompressor;

    private final TileBag mTileBag = new TileBag();

    // request or release the native tileprovider
    private synchronized boolean isOccupied(boolean wantIt) {

        if (!mNativeIsDrawing && wantIt) { // permit to draw
            mNativeIsDrawing = true;
            return false;
        }
        if (mNativeIsDrawing && wantIt) { // refuse to draw
            return true;
        }
        mNativeIsDrawing = false; // release
        return false;
    }


    NavitMapTileProvider(int size) {
        mSize = size;
        mTileCompressor = new TileCompressor();
    }


    @SuppressWarnings("unused")
    synchronized void receiveTile(Bitmap bitmap, int x, int y, int zoom, int rowsCols) {

        //Log.e(TAG, "receiveTile " + bitmap.getWidth() + " " + bitmap.getHeight());

        Log.e(TAG, "received Tile " + zoom + "," + (x) + "," + (y) + " " + rowsCols);

        // create pending tiles
        for (int row = 0; row < rowsCols; row++) {
            for (int col = 0; col < rowsCols; col++) {
                TileXYZp tileP = new TileXYZp(x + col, y + row, zoom, null, null);
                mTileBag.put(tileP.toString(), tileP);
            }
        }

        for (int row = 0; row < rowsCols; row++) {
            for (int col = 0; col < rowsCols; col++) {

                Bitmap part = Bitmap.createBitmap(bitmap, (col * mSize), (row * mSize), mSize, mSize);

                Log.e(TAG, "received Tile is compressed to PNG " + zoom + "," + (x + col) + "," + (y + row));

                TileXYZp tileP = new TileXYZp(x + col, y + row, zoom, null, part);
                mTileBag.put(tileP.toString(), tileP);
                mTileCompressor.mQueUe.offer(tileP);

            }
        }

        isOccupied(false);
    }


    // this gets called from a bunch of different threads !!
    @Override
    public synchronized Tile getTile(int x, int y, int zoom) {

        Log.e(TAG, "getTile z" + zoom + " x" + x + " y" + y);
        Tile tile = null;
        if (zoom > 20 || zoom < 6) {
            Log.e(TAG, "asked zoomlevel out of range");
            return NO_TILE;
        }
        tile = mTileBag.getTile(x,y,zoom);
        if (tile != null) {
            Log.e(TAG, "returning tile from cache");
            return tile;
        }
        if (!isOccupied(true)) {
            int a = getTileOpenGL(mSize, zoom, x, y);
            Log.e(TAG, "asked Tile z " + zoom + " x = " + x + " y = " + y);
            tile = mTileBag.getTile(x,y,zoom);
            if (tile != null) {
                Log.e(TAG, "returning tile");
                return tile;
            } else {
                Log.e(TAG, "NO_TILE");
                return NO_TILE; // caller should not ask again
            }
        } else {
            Log.e(TAG, "no tile to return");
            return null; // caller may ask again later
        }
    }
}
