package org.navitproject.navit;

import android.graphics.Bitmap;
import android.util.Log;

import com.google.android.gms.maps.model.Tile;

import java.io.ByteArrayOutputStream;
import java.lang.Runnable;
import java.lang.Thread;
import java.util.concurrent.LinkedBlockingQueue;

class TileCompressor {

    LinkedBlockingQueue<TileXYZp> mQueUe = new LinkedBlockingQueue<TileXYZp>();

    class MyRunnable implements Runnable {


        private final int mNumber;
        private static final String TAG = "Tilecompressor";

        MyRunnable(int i) {
            super();
            mNumber = i;
        }

        @Override
        public void run() {
            while (true) {
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                TileXYZp tileP = null;
                try {
                    tileP = mQueUe.take();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                //Log.e(TAG, " -- start compression " + mNumber);
                Bitmap bitmap = tileP.mBitmap;
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, baos);
                Tile tile = new Tile(tileP.getSize(), tileP.getSize(), baos.toByteArray());
                tileP.setTile(tile);
                Log.e(TAG, " -- finished a compression (" + mNumber + ") " + tileP.toString());
            }
        }
    }


    TileCompressor() {
        Log.e("compressor", "init\n");
        Thread compressorThread1 = new Thread(new MyRunnable(1));
        compressorThread1.start();
        Thread compressorThread2 = new Thread(new MyRunnable(2));
        compressorThread2.start();
        Thread compressorThread3 = new Thread(new MyRunnable(3));
        compressorThread3.start();
        Thread compressorThread4 = new Thread(new MyRunnable(4));
        compressorThread4.start();
        Thread compressorThread5 = new Thread(new MyRunnable(5));
        compressorThread5.start();
    }

}