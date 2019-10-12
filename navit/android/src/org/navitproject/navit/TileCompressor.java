package org.navitproject.navit;

import android.graphics.Bitmap;
import android.util.Log;

import com.google.android.gms.maps.model.Tile;

import java.io.ByteArrayOutputStream;
import java.lang.Runnable;
import java.lang.Thread;
import java.util.concurrent.LinkedBlockingQueue;

class TileCompressor {

    LinkedBlockingQueue<TileXYZP> queUe = new LinkedBlockingQueue<TileXYZP>();

    class MyRunnable implements Runnable {


        private final int mNumber;
        private final String TAG = "Tilecompressor";

        MyRunnable(int i) {
            super();
            mNumber = i;
        }

        @Override
        public void run() {
            while(true) {
                Log.e(TAG, " -- ready " + mNumber);
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                TileXYZP tileXYZP = null;
                try {
                    tileXYZP = queUe.take();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                Bitmap bitmap = tileXYZP.mBitmap;
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, baos);
                Tile tile = new Tile(tileXYZP.getSize(), tileXYZP.getSize(), baos.toByteArray());
                tileXYZP.setTile(tile);
                Log.e(TAG, " -- finished a compression " + mNumber);
                Log.e(TAG," " +tileXYZP.toString());
            }
        }
    }


    TileCompressor() {
        Thread compressorThread1 = new Thread(new MyRunnable(1));
        compressorThread1.start();
        Thread compressorThread2 = new Thread(new MyRunnable(2));
        compressorThread2.start();
        Thread compressorThread3 = new Thread(new MyRunnable(3));
        compressorThread3.start();
    }

}