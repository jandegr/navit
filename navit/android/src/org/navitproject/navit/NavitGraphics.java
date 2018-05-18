/**
 * Navit, a modular navigation system. Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

package org.navitproject.navit;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PointF;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.ContextMenu;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageButton;
import android.widget.RelativeLayout;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.ArrayList;


public class NavitGraphics {

    /* These constants must be synchronized with enum draw_mode_num in graphics.h. */
    private static final int draw_mode_begin = 0;
    private static final int draw_mode_end = 1;
    private static final String TAG = "NavitGraphics";
    private static Boolean in_map = false;
    private static final MsgType[] msg_values = MsgType.values();
    // for menu key
    private static final long time_for_long_press = 300L;
    static final Handler callbackHandler = new NavitGrahicsHandler();
    private int bitmapW;
    private int bitmapH;
    private int posX;
    private int posY;
    private int posWraparound;
    private int overlayDisabled;
    private float trackballX;
    private float trackballY;
    private View view;
    private Activity activity;
    private ImageButton zoomInButton;
    private ImageButton zoomOutButton;
    private final NavitGraphics parentGraphics;
    private final ArrayList<NavitGraphics> overlays = new ArrayList<>();

    private Canvas drawCanvas;
    private Bitmap drawBitmap;
    private int sizeChangedCallbackID;
    private int buttonCallbackID;
    private int motionCallbackID;
    private int keypressCallbackID;

    /**
     * Constructs a NavitGraphics object.
     *
     * @param activity a Navit instance
     * @param parent parent if an overlay is to be constructed
     * @param x position x on screen
     * @param y position y on screen
     * @param w width
     * @param h height
     * @param alpha not used
     * @param wraparound to be clarified
     */
    public NavitGraphics(final Activity activity, NavitGraphics parent, int x, int y, int w, int h,
                         @SuppressWarnings("unused") int alpha, int wraparound) {
        if (parent == null) {
            this.activity = activity;
            view = new NavitView(activity);
            //activity.registerForContextMenu(view);
            view.setClickable(false);
            view.setFocusable(true);
            view.setFocusableInTouchMode(true);
            view.setKeepScreenOn(true);
            RelativeLayout relativelayout = new RelativeLayout(activity);
            relativelayout.addView(view);

            RelativeLayout.LayoutParams lpLeft = new RelativeLayout.LayoutParams(96,96);

            lpLeft.addRule(RelativeLayout.ALIGN_PARENT_LEFT);


            zoomOutButton = new ImageButton(activity);
            zoomOutButton.setLayoutParams(lpLeft);
            zoomOutButton.setBackgroundResource(R.drawable.zoom_out);
            zoomOutButton.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    callbackMessageChannel(2, "");
                }
            });

            zoomInButton = new ImageButton(activity);
            RelativeLayout.LayoutParams lpRight = new RelativeLayout.LayoutParams(96,96);
            lpRight.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
            zoomInButton.setLayoutParams(lpRight);
            zoomInButton.setBackgroundResource(R.drawable.zoom_in);
            zoomInButton.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    callbackMessageChannel(1, "");
                }
            });

            relativelayout.addView(zoomInButton);
            relativelayout.addView(zoomOutButton);
            activity.setContentView(relativelayout);
            view.requestFocus();
        } else {
            drawBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            bitmapW = w;
            bitmapH = h;
            posX = x;
            posY = y;
            posWraparound = wraparound;
            drawCanvas = new Canvas(drawBitmap);
            parent.overlays.add(this);
        }
        parentGraphics = parent;
    }

    public static native String[][] getAllCountries();

    private Rect get_rect() {
        Rect ret = new Rect();
        ret.left = posX;
        ret.top = posY;
        if (posWraparound != 0) {
            if (ret.left < 0) {
                ret.left += parentGraphics.bitmapW;
            }
            if (ret.top < 0) {
                ret.top += parentGraphics.bitmapH;
            }
        }
        ret.right = ret.left + bitmapW;
        ret.bottom = ret.top + bitmapH;
        if (posWraparound != 0) {
            if (bitmapW < 0) {
                ret.right = ret.left + bitmapW + parentGraphics.bitmapW;
            }
            if (bitmapH < 0) {
                ret.bottom = ret.top + bitmapH + parentGraphics.bitmapH;
            }
        }
        return ret;
    }

    public native void sizeChangedCallback(int id, int x, int y);

    public native void keypressCallback(int id, String s);

    public static native int callbackMessageChannel(int i, String s);

    public native void buttonCallback(int id, int pressed, int button, int x, int y);

    public native void motionCallback(int id, int x, int y);

    public native String getDefaultCountry(int id, String s);
    // private int count;

    public void setSizeChangedCallback(int id) {
        sizeChangedCallbackID = id;
    }

    public void setButtonCallback(int id) {
        buttonCallbackID = id;
    }

    public void setMotionCallback(int id) {
        motionCallbackID = id;
        Navit.setMotionCallback(id, this);
    }

    public void setKeypressCallback(int id) {
        keypressCallbackID = id;
        // set callback id also in main intent (for menus)
        Navit.setKeypressCallback(id, this);
    }


    protected void draw_polyline(Paint paint, int[] c) {

        //  Log.e("NavitGraphics","draw_polyline");
        paint.setStrokeWidth(c[0]);
        paint.setARGB(c[1], c[2], c[3], c[4]);
        paint.setStyle(Paint.Style.STROKE);
        //paint.setAntiAlias(true);
        //paint.setStrokeWidth(0);

        int ndashes;
        float[] intervals;

        ndashes = c[5];
        intervals = new float[ndashes + (ndashes % 2)];
        for (int i = 0; i < ndashes; i++) {
            intervals[i] = c[6 + i];
        }

        if ((ndashes % 2) == 1) {
            intervals[ndashes] = intervals[ndashes - 1];
        }

        if (ndashes > 0) {
            paint.setPathEffect(new android.graphics.DashPathEffect(intervals, 0.0f));
        }

        Path path = new Path();
        path.moveTo(c[6 + ndashes], c[7 + ndashes]);
        for (int i = 8 + ndashes; i < c.length; i += 2) {
            path.lineTo(c[i], c[i + 1]);
        }
        //global_path.close();
        drawCanvas.drawPath(path, paint);
        paint.setPathEffect(null);
    }

    protected void draw_polygon(Paint paint, int[] c) {
        //Log.e("NavitGraphics","draw_polygon");
        paint.setStrokeWidth(c[0]);
        paint.setARGB(c[1], c[2], c[3], c[4]);
        paint.setStyle(Paint.Style.FILL);
        //paint.setAntiAlias(true);
        //paint.setStrokeWidth(0);
        Path path = new Path();
        path.moveTo(c[5], c[6]);
        for (int i = 7; i < c.length; i += 2) {
            path.lineTo(c[i], c[i + 1]);
        }
        //global_path.close();
        drawCanvas.drawPath(path, paint);
    }

    protected void draw_rectangle(Paint paint, int x, int y, int w, int h) {
        //Log.e("NavitGraphics","draw_rectangle");
        Rect r = new Rect(x, y, x + w, y + h);
        paint.setStyle(Paint.Style.FILL);
        paint.setAntiAlias(true);
        //paint.setStrokeWidth(0);
        drawCanvas.drawRect(r, paint);
    }

    protected void draw_circle(Paint paint, int x, int y, int r) {
        //Log.e("NavitGraphics","draw_circle");
        //      float fx = x;
        //      float fy = y;
        //      float fr = r / 2;
        paint.setStyle(Paint.Style.STROKE);
        drawCanvas.drawCircle(x, y, r / 2, paint);
    }

    protected void draw_text(Paint paint, int x, int y, String text, int size, int dx, int dy, int bgcolor) {
        int oldcolor = paint.getColor();
        Path path = null;

        paint.setTextSize(size / 15);
        paint.setStyle(Paint.Style.FILL);

        if (dx != 0x10000 || dy != 0) {
            path = new Path();
            path.moveTo(x, y);
            path.rLineTo(dx, dy);
            paint.setTextAlign(android.graphics.Paint.Align.LEFT);
        }

        if (bgcolor != 0) {
            paint.setStrokeWidth(3);
            paint.setColor(bgcolor);
            paint.setStyle(Paint.Style.STROKE);
            if (path == null) {
                drawCanvas.drawText(text, x, y, paint);
            } else {
                drawCanvas.drawTextOnPath(text, path, 0, 0, paint);
            }
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(oldcolor);
        }

        if (path == null) {
            drawCanvas.drawText(text, x, y, paint);
        } else {
            drawCanvas.drawTextOnPath(text, path, 0, 0, paint);
        }
        paint.clearShadowLayer();
    }

    protected void draw_image(Paint paint, int x, int y, Bitmap bitmap) {
        //Log.e("NavitGraphics","draw_image");
        //      float fx = x;
        //      float fy = y;
        drawCanvas.drawBitmap(bitmap, x, y, paint);
    }

    /* takes an image and draws it on the screen as a prerendered maptile
     *
     *
     *
     * @param imagepath     a string representing an absolute or relative path
     *                      to the image file
     * @param count         the number of points specified
     * @param p0x and p0y   specifying the top left point
     * @param p1x and p1y   specifying the top right point
     * @param p2x and p2y   specifying the bottom left point, not yet used but kept
     *                      for compatibility with the linux port
     *
     * TODO make it work with 4 points specified to make it work for 3D mapview, so it can be used
     *      for small but very detailed maps as well as for large maps with very little detail but large
     *      coverage.
     * TODO make it work with rectangular tiles as well ?
     */
    protected void draw_image_warp(String imagepath, int count, int p0x, int p0y, int p1x, int p1y, int p2x, int p2y) {
        //  if (!isAccelerated)
        //  {
        Log.e("NavitGraphics", "path = " + imagepath);
        Log.e("NavitGraphics", "count = " + count);
        Log.e("NavitGraphics", "NMPpath = " + Navit.map_filename_path);
        float width;
        float scale;
        float deltaY;
        float deltaX;
        float angle;
        Bitmap bitmap = null;
        FileInputStream infile;
        Matrix matrix;

        if (count == 3) {
            if (!imagepath.startsWith("/")) {
                imagepath = Navit.map_filename_path + imagepath;
            }
            Log.e("NavitGraphics", "pathc3 = " + imagepath);
            try {
                infile = new FileInputStream(imagepath);
                bitmap = BitmapFactory.decodeStream(infile);
                infile.close();
            } catch (IOException e) {
                Log.e("NavitGraphics", "could not open " + imagepath);
            }
            if (bitmap != null) {
                matrix = new Matrix();
                deltaX = p1x - p0x;
                deltaY = p1y - p0y;
                width = (float) (Math.sqrt((deltaX * deltaX) + (deltaY * deltaY)));
                angle = (float) (Math.atan2(deltaY, deltaX) * 180d / Math.PI);
                scale = width / bitmap.getWidth();
                matrix.preScale(scale, scale);
                matrix.postTranslate(p0x, p0y);
                matrix.postRotate(angle, p0x, p0y);
                drawCanvas.drawBitmap(bitmap, matrix, null);
            }
        }
        //  }
        //  else
        //      ((NavitCanvas)draw_canvas).draw_image_warp(imagepath,count,p0x,p0y,p1x,p1y,p2x,p2y);

    }

    protected void draw_mode(int mode) {
        //Log.e("NavitGraphics", "draw_mode mode=" + mode + " parent_graphics="
        //      + String.valueOf(parent_graphics));

        if (mode == draw_mode_end) {
            if (parentGraphics == null) {
                if (overlayDisabled == 0) {
                    this.setButtonState(true);
                } else {
                    this.setButtonState(false);
                }
                view.invalidate();
            } else {
                parentGraphics.view.invalidate(get_rect());
            }
        }
        if (mode == draw_mode_begin && parentGraphics != null) {
            drawBitmap.eraseColor(0);
        }

    }

    //allows the buttons to be removed
    //in case a navit navitve gui is accessed
    private void setButtonState(boolean b) {
        this.zoomInButton.setEnabled(b);
        this.zoomOutButton.setEnabled(b);
        if (b) {
            this.zoomInButton.setVisibility(View.VISIBLE);
            this.zoomOutButton.setVisibility(View.VISIBLE);
        } else {
            this.zoomInButton.setVisibility(View.INVISIBLE);
            this.zoomOutButton.setVisibility(View.INVISIBLE);
        }
        Log.d("Navitgraphics", "setbuttons " + b);
    }

    protected void draw_drag(int x, int y) {
        //Log.e("NavitGraphics","draw_drag");
        posX = x;
        posY = y;
    }

    protected void overlay_disable(int disable) {
        Log.e("NavitGraphics", "overlay_disable: " + disable + "Parent: " + (parentGraphics != null));
        // assume we are NOT in map view mode!
        if (parentGraphics == null) {
            in_map = (disable == 0);
        }
        if (overlayDisabled != disable) {
            overlayDisabled = disable;
            if (parentGraphics != null) {
                parentGraphics.view.invalidate(get_rect());
            }
        }
    }

    protected void overlay_resize(int x, int y, int w, int h, @SuppressWarnings("unused") int alpha, int wraparound) {
        //Log.e("NavitGraphics","overlay_resize");
        drawBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        bitmapW = w;
        bitmapH = h;
        posX = x;
        posY = y;
        posWraparound = wraparound;
        drawCanvas.setBitmap(drawBitmap);
    }

    public enum MsgType {
        CLB_ZOOM_IN, CLB_ZOOM_OUT, CLB_REDRAW, CLB_MOVE, CLB_BUTTON_UP, CLB_BUTTON_DOWN, CLB_SET_DESTINATION,
        CLB_SET_DISPLAY_DESTINATION, CLB_CALL_CMD, CLB_COUNTRY_CHOOSER, CLB_LOAD_MAP, CLB_UNLOAD_MAP,
        CLB_DELETE_MAP, CLB_ABORT_NAVIGATION, CLB_BLOCK, CLB_UNBLOCK
    }

    private static class NavitGrahicsHandler extends Handler {
        public void handleMessage(Message msg) {
            switch (msg_values[msg.what]) {
                case CLB_ZOOM_IN:
                    callbackMessageChannel(1, "");
                    break;
                case CLB_ZOOM_OUT:
                    callbackMessageChannel(2, "");
                    break;
                case CLB_REDRAW:
                    break;
                case CLB_MOVE:
                    //MotionCallback(MotionCallbackID, msg.getData().getInt("x"), msg.getData().getInt("y"));
                    break;
                case CLB_SET_DESTINATION:
                    String lat = Float.toString(msg.getData().getFloat("lat"));
                    String lon = Float.toString(msg.getData().getFloat("lon"));
                    String q = msg.getData().getString(("q"));
                    callbackMessageChannel(3, lat + "#" + lon + "#" + q);
                    break;
                case CLB_SET_DISPLAY_DESTINATION:
                    Log.e("TAG", "CLB_SET_DISPLAY_DESTINATION");
                    int x = msg.arg1;
                    int y = msg.arg2;
                    callbackMessageChannel(4, "" + x + "#" + y);
                    Log.e(TAG, ("" + x + "#" + y));
                    // weet niet of dit wel werkt
                    break;
                case CLB_CALL_CMD:
                    String cmd = msg.getData().getString(("cmd"));
                    callbackMessageChannel(5, cmd);
                    Log.w(TAG, "CLB_CALL_CMD " + cmd);
                    break;
                case CLB_BUTTON_UP:
                    //ButtonCallback(ButtonCallbackID, 0, 1, msg.getData().getInt("x"), msg.getData().getInt("y"));
                    // up
                    break;
                case CLB_BUTTON_DOWN:
                    //ButtonCallback(ButtonCallbackID, 1, 1, msg.getData().getInt("x"), msg.getData().getInt("y"));
                    // down
                    break;
                case CLB_COUNTRY_CHOOSER:
                    break;
                case CLB_ABORT_NAVIGATION:
                    callbackMessageChannel(3, "");
                    break;
                case CLB_LOAD_MAP:
                    Integer ret = callbackMessageChannel(6, msg.getData().getString(("title")));
                    Log.e(TAG, "callBackRet = " + ret);
                    break;
                case CLB_DELETE_MAP:
                    File toDelete = new File(msg.getData().getString(("title")));
                    Log.e("deletefile", "" + toDelete.getPath() + " " + toDelete.getName());
                    toDelete.delete();
                    //fallthrough
                case CLB_UNLOAD_MAP:
                    Log.e(TAG, "CLB_UNLOAD_MAP");
                    callbackMessageChannel(7, msg.getData().getString(("title")));
                    break;
                case CLB_BLOCK:
                    Log.e(TAG, "CLB_BLOCK");
                    callbackMessageChannel(8, "");
                    break;
                case CLB_UNBLOCK:
                    Log.e(TAG, "CLB_UNBLOCK");
                    callbackMessageChannel(9, "");
                    break;
                default:
                    break;
            }
        }
    }

    private class NavitView extends View implements Runnable, MenuItem.OnMenuItemClickListener {

        static final int NONE = 0;
        static final int DRAG = 1;
        static final int ZOOM = 2;
        static final int PRESSED = 3;
        PointF mPressedPosition = null;
        int touchMode = NONE;
        float oldDist = 0;
        Method eventGetX = null;
        Method eventGetY = null;

        public NavitView(Context context) {
            super(context);
            try {
                eventGetX = android.view.MotionEvent.class.getMethod("getX", int.class);
                eventGetY = android.view.MotionEvent.class.getMethod("getY", int.class);
            } catch (Exception e) {
                Log.e("NavitGraphics", "Multitouch zoom not supported");
            }
        }

        @Override
        protected void onCreateContextMenu(ContextMenu menu) {
            super.onCreateContextMenu(menu);
            menu.setHeaderTitle(Navit.getInstance().getTstring(R.string.position_popup_title) + "..");
            menu.add(1, 1, NONE, Navit.getInstance().getTstring(R.string.position_popup_drive_here))
                    .setOnMenuItemClickListener(this);
            menu.add(1, 2, NONE, Navit.getInstance().getTstring(R.string.cancel)).setOnMenuItemClickListener(this);
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            switch (item.getItemId()) {
                case 1:
                    Message msg = Message.obtain(callbackHandler, MsgType.CLB_SET_DISPLAY_DESTINATION.ordinal(),
                            (int) mPressedPosition.x, (int) mPressedPosition.y);
                    msg.sendToTarget();
                    break;
                default:
                    break;
            }
            return false;
        }


        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            canvas.drawBitmap(drawBitmap, posX, posY, null);
            if (overlayDisabled == 0) {
                // assume we ARE in map view mode!
                in_map = true;
                for (NavitGraphics overlay : overlays) {
                    if (overlay.overlayDisabled == 0) {
                        Rect r = overlay.get_rect();
                        canvas.drawBitmap(overlay.drawBitmap, r.left, r.top, null);
                    }
                }
            } else {
                if (Navit.show_soft_keyboard) {
                    if (Navit.mgr != null) {
                        //Log.e("NavitGraphics", "view -> SHOW SoftInput");
                        //Log.e("NavitGraphics", "view mgr=" + String.valueOf(Navit.mgr));
                        Navit.mgr.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT);
                        Navit.show_soft_keyboard_now_showing = true;
                        // clear the variable now, keyboard will stay on screen until backbutton pressed
                        Navit.show_soft_keyboard = false;
                    }
                }
            }
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            Log.e("Navit", "NavitGraphics -> onSizeChanged pixels x=" + w + " pixels y=" + h);
            Log.e("Navit", "NavitGraphics -> onSizeChanged density=" + Navit.metrics.density);
            Log.e("Navit", "NavitGraphics -> onSizeChanged scaledDensity="
                    + Navit.metrics.scaledDensity);
            super.onSizeChanged(w, h, oldw, oldh);
            drawBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            drawCanvas = new Canvas(drawBitmap);
            bitmapW = w;
            bitmapH = h;
            sizeChangedCallback(sizeChangedCallbackID, w, h);
        }

        void do_longpress_action() {
            Log.e("NavitGraphics", "do_longpress_action enter");

            activity.openContextMenu(this);
        }

        private int getActionField(String fieldname, Object obj) {
            int retValue = -999;
            try {
                java.lang.reflect.Field field = android.view.MotionEvent.class.getField(fieldname);
                try {
                    retValue = field.getInt(obj);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            } catch (NoSuchFieldException ex) {
            }

            return retValue;
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            //Log.e("NavitGraphics", "onTouchEvent");
            super.onTouchEvent(event);
            int x = (int) event.getX();
            int y = (int) event.getY();

            int _ACTION_POINTER_UP_ = getActionField("ACTION_POINTER_UP", event);
            int _ACTION_POINTER_DOWN_ = getActionField("ACTION_POINTER_DOWN", event);
            int _ACTION_MASK_ = getActionField("ACTION_MASK", event);

            int switchValue = event.getAction();
            if (_ACTION_MASK_ != -999) {
                switchValue = (event.getAction() & _ACTION_MASK_);
            }

            if (switchValue == MotionEvent.ACTION_DOWN) {
                touchMode = PRESSED;
                if (!in_map) {
                    buttonCallback(buttonCallbackID, 1, 1, x, y); // down
                }
                mPressedPosition = new PointF(x, y);
                postDelayed(this, time_for_long_press);
            } else if ((switchValue == MotionEvent.ACTION_UP) || (switchValue == _ACTION_POINTER_UP_)) {
                Log.e("NavitGraphics", "ACTION_UP");

                switch (touchMode) {
                    case DRAG:
                        Log.e("NavitGraphics", "onTouch move");

                        motionCallback(motionCallbackID, x, y);
                        buttonCallback(buttonCallbackID, 0, 1, x, y); // up

                        break;
                    case ZOOM:
                        //Log.e("NavitGraphics", "onTouch zoom");

                        float newDist = spacing(getFloatValue(event, 0), getFloatValue(event, 1));
                        float scale = 0;
                        if (newDist > 10f) {
                            scale = newDist / oldDist;
                        }

                        if (scale > 1.3) {
                            // zoom in
                            callbackMessageChannel(1, null);
                            //Log.e("NavitGraphics", "onTouch zoom in");
                        } else if (scale < 0.8) {
                            // zoom out
                            callbackMessageChannel(2, null);
                            //Log.e("NavitGraphics", "onTouch zoom out");
                        }
                        break;
                    case PRESSED:
                        if (in_map) {
                            buttonCallback(buttonCallbackID, 1, 1, x, y); // down
                        }
                        buttonCallback(buttonCallbackID, 0, 1, x, y); // up
                        break;
                    default:
                        break;
                }
                touchMode = NONE;
            } else if (switchValue == MotionEvent.ACTION_MOVE) {
                //Log.e("NavitGraphics", "ACTION_MOVE");

                switch (touchMode) {
                    case DRAG:
                        motionCallback(motionCallbackID, x, y);
                        break;
                    case ZOOM:
                        float newDist = spacing(getFloatValue(event, 0), getFloatValue(event, 1));
                        float scale = newDist / oldDist;
                        Log.e("NavitGraphics", "New scale = " + scale);
                        if (scale > 1.2) {
                            // zoom in
                            callbackMessageChannel(1, "");
                            oldDist = newDist;
                            //Log.e("NavitGraphics", "onTouch zoom in");
                        } else if (scale < 0.8) {
                            oldDist = newDist;
                            // zoom out
                            callbackMessageChannel(2, "");
                            //Log.e("NavitGraphics", "onTouch zoom out");
                        }
                        break;
                    case PRESSED:
                        Log.e("NavitGraphics", "Start drag mode");
                        if (spacing(mPressedPosition, new PointF(event.getX(), event.getY())) > 20f) {
                            buttonCallback(buttonCallbackID, 1, 1, x, y); // down
                            touchMode = DRAG;
                        }
                        break;
                    default:
                        break;
                }
            } else if (switchValue == _ACTION_POINTER_DOWN_) {
                //Log.e("NavitGraphics", "ACTION_POINTER_DOWN");
                oldDist = spacing(getFloatValue(event, 0), getFloatValue(event, 1));
                if (oldDist > 2f) {
                    touchMode = ZOOM;
                    //Log.e("NavitGraphics", "--> zoom");
                }
            }
            return true;
        }

        private float spacing(PointF a, PointF b) {
            float x = a.x - b.x;
            float y = a.y - b.y;
            return (float) Math.sqrt(x * x + y * y);
        }

        private PointF getFloatValue(Object instance, Object argument) {
            PointF pos = new PointF(0, 0);
            if (eventGetX != null && eventGetY != null) {
                try {
                    Float x = (java.lang.Float) eventGetX.invoke(instance, argument);
                    Float y = (java.lang.Float) eventGetY.invoke(instance, argument);
                    pos.set(x, y);
                } catch (Exception e) {
                }
            }
            return pos;
        }

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            int i;
            String s = null;
            boolean handled = true;
            i = event.getUnicodeChar();
            //Log.e("NavitGraphics", "onKeyDown " + keyCode + " " + i);
            // Log.e("NavitGraphics","Unicode "+event.getUnicodeChar());
            if (i == 0) {
                switch (keyCode) {
                    case KeyEvent.KEYCODE_DEL:
                        s = String.valueOf((char) 8);
                        break;
                    case KeyEvent.KEYCODE_MENU:
                        if (!in_map) {
                            // if last menukeypress is less than 0.2 seconds away then count longpress
                            final long intervalForLongPress = 200L;
                            if ((System.currentTimeMillis() - Navit.last_pressed_menu_key) < intervalForLongPress) {
                                Navit.time_pressed_menu_key = Navit.time_pressed_menu_key
                                        + (System.currentTimeMillis() - Navit.last_pressed_menu_key);
                                //Log.e("NavitGraphics", "press time=" + Navit.time_pressed_menu_key);

                                // on long press let softkeyboard popup
                                if (Navit.time_pressed_menu_key > time_for_long_press) {
                                    //Log.e("NavitGraphics", "long press menu key!!");
                                    Navit.show_soft_keyboard = true;
                                    Navit.time_pressed_menu_key = 0L;
                                    // need to draw to get the keyboard showing
                                    this.postInvalidate();
                                }
                            } else {
                                Navit.time_pressed_menu_key = 0L;
                            }
                            Navit.last_pressed_menu_key = System.currentTimeMillis();
                            // if in menu view:
                            // use as OK (Enter) key
                            // s = String.valueOf((char) 13);
                            handled = true;
                            // dont use menu key here (use it in onKeyUp)
                            return handled;
                        } else {
                            // if on map view:
                            // volume UP
                            //s = java.lang.String.valueOf((char) 1);
                            handled = false;
                            return handled;
                        }
                    case KeyEvent.KEYCODE_SEARCH:
                        /* Handle event in Main Activity if map is shown */
                        if (in_map) {
                            return false;
                        }

                        s = String.valueOf((char) 19);
                        break;
                    case KeyEvent.KEYCODE_BACK:
                        //Log.e("NavitGraphics", "KEYCODE_BACK down");
                        s = String.valueOf((char) 27);
                        break;
                    case KeyEvent.KEYCODE_CALL:
                        s = String.valueOf((char) 3);
                        break;
                    case KeyEvent.KEYCODE_VOLUME_UP:
                        if (!in_map) {
                            // if in menu view:
                            // use as UP key
                            s = String.valueOf((char) 16);
                            handled = true;
                        } else {
                            // if on map view:
                            // volume UP
                            //s = java.lang.String.valueOf((char) 21);
                            handled = false;
                            return handled;
                        }
                        break;
                    case KeyEvent.KEYCODE_VOLUME_DOWN:
                        if (!in_map) {
                            // if in menu view:
                            // use as DOWN key
                            s = String.valueOf((char) 14);
                            handled = true;
                        } else {
                            // if on map view:
                            // volume DOWN
                            //s = java.lang.String.valueOf((char) 4);
                            handled = false;
                            return handled;
                        }
                        break;
                    case KeyEvent.KEYCODE_DPAD_CENTER:
                        s = String.valueOf((char) 13);
                        break;
                    case KeyEvent.KEYCODE_DPAD_DOWN:
                        s = String.valueOf((char) 16);
                        break;
                    case KeyEvent.KEYCODE_DPAD_LEFT:
                        s = String.valueOf((char) 2);
                        break;
                    case KeyEvent.KEYCODE_DPAD_RIGHT:
                        s = String.valueOf((char) 6);
                        break;
                    case KeyEvent.KEYCODE_DPAD_UP:
                        s = String.valueOf((char) 14);
                        break;
                    default:
                        break;
                }
            } else if (i == 10) {
                s = java.lang.String.valueOf((char) 13);
            }

            if (s != null) {
                keypressCallback(keypressCallbackID, s);
            }
            return handled;
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            //Log.e("NavitGraphics", "onKeyUp " + keyCode);

            int i;
            String s = null;
            boolean handled = true;
            i = event.getUnicodeChar();

            if (i == 0) {
                switch (keyCode) {
                    case KeyEvent.KEYCODE_VOLUME_UP:
                        if (!in_map) {
                            //s = java.lang.String.valueOf((char) 16);
                            handled = true;
                            return handled;
                        } else {
                            //s = java.lang.String.valueOf((char) 21);
                            handled = false;
                            return handled;
                        }
                    case KeyEvent.KEYCODE_VOLUME_DOWN:
                        if (!in_map) {
                            //s = java.lang.String.valueOf((char) 14);
                            handled = true;
                            return handled;
                        } else {
                            //s = java.lang.String.valueOf((char) 4);
                            handled = false;
                            return handled;
                        }
                    case KeyEvent.KEYCODE_SEARCH:
                        /* Handle event in Main Activity if map is shown */
                        if (in_map) {
                            return false;
                        }
                        break;
                    case KeyEvent.KEYCODE_BACK:
                        if (Navit.show_soft_keyboard_now_showing) {
                            Navit.show_soft_keyboard_now_showing = false;
                        }
                        //Log.e("NavitGraphics", "KEYCODE_BACK up");
                        //s = java.lang.String.valueOf((char) 27);
                        handled = true;
                        return handled;
                    case KeyEvent.KEYCODE_MENU:
                        if (!in_map && !Navit.show_soft_keyboard_now_showing) {
                            // if soft keyboard showing on screen, don't use menu button as select key
                            // if in menu view:
                            // use as OK (Enter) key
                            s = String.valueOf((char) 13);
                            handled = true;
                        } else {
                            // if on map view:
                            // volume UP
                            //s = java.lang.String.valueOf((char) 1);
                            handled = false;
                            return handled;
                        }
                        break;
                    default:
                        break;
                }
            } else if (i != 10) {
                s = java.lang.String.valueOf((char) i);
            }

            if (s != null) {
                keypressCallback(keypressCallbackID, s);
            }
            return handled;

        }

        @Override
        public boolean onKeyMultiple(int keyCode, int count, KeyEvent event) {
            String s;
            if (keyCode == KeyEvent.KEYCODE_UNKNOWN) {
                s = event.getCharacters();
                keypressCallback(keypressCallbackID, s);
                return true;
            }
            return super.onKeyMultiple(keyCode, count, event);
        }

        @Override
        public boolean onTrackballEvent(MotionEvent event) {
            //Log.e("NavitGraphics", "onTrackball " + event.getAction() + " " + event.getX() + " "
            //      + event.getY());
            String s = null;
            if (event.getAction() == android.view.MotionEvent.ACTION_DOWN) {
                s = java.lang.String.valueOf((char) 13);
            }
            if (event.getAction() == android.view.MotionEvent.ACTION_MOVE) {
                trackballX += event.getX();
                trackballY += event.getY();
                //Log.e("NavitGraphics", "trackball " + trackball_x + " " + trackball_y);
                if (trackballX <= -1) {
                    s = java.lang.String.valueOf((char) 2);
                    trackballX += 1;
                }
                if (trackballX >= 1) {
                    s = java.lang.String.valueOf((char) 6);
                    trackballX -= 1;
                }
                if (trackballY <= -1) {
                    s = java.lang.String.valueOf((char) 16);
                    trackballY += 1;
                }
                if (trackballY >= 1) {
                    s = java.lang.String.valueOf((char) 14);
                    trackballY -= 1;
                }
            }
            if (s != null) {
                keypressCallback(keypressCallbackID, s);
            }
            return true;
        }

        public void run() {
            if (in_map && touchMode == PRESSED) {
                do_longpress_action();
                touchMode = NONE;
            }
        }

    }

}
