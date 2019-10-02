/*
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

package org.navitproject.navit;

import static org.navitproject.navit.NavitAppConfig.getTstring;

import android.annotation.TargetApi;
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
import android.os.Message;
import android.util.Log;
import android.view.ContextMenu;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowInsets;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageButton;
import android.widget.RelativeLayout;

import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;


class NavitGraphics {
    private static final String            TAG = "NavitGraphics";
    private final NavitGraphics            mParentGraphics;
    private final ArrayList<NavitGraphics> mOverlays;
    private int                            mBitmapWidth;
    private int                            mBitmapHeight;
    private int                            mPosX;
    private int                            mPosY;
    private int                            mPosWraparound;
    private int                            mOverlayDisabled;
    private float                          mTrackballX;
    private float                          mTrackballY;
    private ImageButton                    mZoomInButton;
    private ImageButton                    mZoomOutButton;
    private View                           mView;
    private RelativeLayout                 mRelativeLayout;
    private NavitCamera                    mCamera;
    private Navit                          mActivity;
    private static boolean                 sInMap;


    private void setCamera(int useCamera) {
        Log.e(TAG,"setCamera = " + useCamera);
        if (useCamera != 0 && mCamera == null) {
            Log.e(TAG,"setCamera = " + useCamera);
            // mActivity.requestWindowFeature(Window.FEATURE_NO_TITLE);
            mCamera = new NavitCamera(mActivity);
            mRelativeLayout.addView(mCamera);
            mRelativeLayout.bringChildToFront(mView);
        }
    }

    private Rect get_rect() {
        Rect ret = new Rect();
        ret.left = mPosX;
        ret.top = mPosY;
        if (mPosWraparound != 0) {
            if (ret.left < 0) {
                ret.left += mParentGraphics.mBitmapWidth;
            }
            if (ret.top < 0) {
                ret.top += mParentGraphics.mBitmapHeight;
            }
        }
        ret.right = ret.left + mBitmapWidth;
        ret.bottom = ret.top + mBitmapHeight;
        if (mPosWraparound != 0) {
            if (mBitmapWidth < 0) {
                ret.right = ret.left + mBitmapWidth + mParentGraphics.mBitmapWidth;
            }
            if (mBitmapHeight < 0) {
                ret.bottom = ret.top + mBitmapHeight + mParentGraphics.mBitmapHeight;
            }
        }
        return ret;
    }

    private class NavitView extends View implements MenuItem.OnMenuItemClickListener,
            GestureDetector.OnGestureListener {
        private final GestureDetector mDetector;
        float             mOldDist = 0;
        static final int  NONE       = 0;

        PointF mPressedPosition = null;

        NavitView(Context context) {
            super(context);
            mDetector = new GestureDetector(context,this);
        }

        public void onWindowFocusChanged(boolean hasWindowFocus) {
            Log.e(TAG,"onWindowFocusChanged = " + hasWindowFocus);
            // beter aanroepen in Navit of appconfig ?
            if (Navit.sShowSoftKeyboardShowing && hasWindowFocus) {
                InputMethodManager imm  = (InputMethodManager) mActivity
                        .getSystemService(Context.INPUT_METHOD_SERVICE);
                imm.showSoftInput(this,InputMethodManager.SHOW_FORCED);
            }
            if (Navit.sShowSoftKeyboardShowing && !hasWindowFocus) {
                InputMethodManager imm  = (InputMethodManager) mActivity
                        .getSystemService(Context.INPUT_METHOD_SERVICE);
                imm.hideSoftInputFromWindow(this.getWindowToken(), 0);
            }
        }


        @TargetApi(21)
        public WindowInsets onApplyWindowInsets(WindowInsets insets) {
            Log.e(TAG,"onApplyWindowInsets");
            //if (mTinting) {
            //    mPaddingLeft = insets.getSystemWindowInsetLeft();
            //    mPaddingRight = insets.getSystemWindowInsetRight();
            //    mPaddingBottom = insets.getSystemWindowInsetBottom();
            //    mPaddingTop = insets.getSystemWindowInsetTop();
            //    Log.e(TAG, String.format("Padding -1a- left=%d top=%d right=%d bottom=%d",
            //            mPaddingLeft, mPaddingTop, mPaddingRight, mPaddingBottom));
            //    int width = this.getWidth();
            //    int height = this.getHeight();
            //    if (width > 0 && height > 0) {
            //        sizeChangedCallback(mSizeChangedCallbackID, width, height);
            //    }
            //}
            return insets;
        }

        @Override
        protected void onCreateContextMenu(ContextMenu menu) {
            super.onCreateContextMenu(menu);

            menu.setHeaderTitle(getTstring(R.string.position_popup_title) + "..");
            menu.add(1, 1, NONE, getTstring(R.string.position_popup_drive_here))
                    .setOnMenuItemClickListener(this);
            menu.add(1, 2, NONE, getTstring(R.string.cancel))
                    .setOnMenuItemClickListener(this);
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            if (item.getItemId() == 1) {
                Message msg = Message.obtain(CallBackHandler.sCallbackHandler,
                        CallBackHandler.MsgType.CLB_SET_DISPLAY_DESTINATION.ordinal(),
                        (int) mPressedPosition.x, (int) mPressedPosition.y);
                msg.sendToTarget();
            }
            return true;
        }


        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            canvas.drawBitmap(mDrawBitmap, mPosX, mPosY, null);
            if (mOverlayDisabled == 0) {
                // assume we ARE in map view mode!
                sInMap = true;
                assert mOverlays != null;
                for (NavitGraphics overlay : mOverlays) {
                    if (overlay.mOverlayDisabled == 0) {
                        Rect r = overlay.get_rect();
                        canvas.drawBitmap(overlay.mDrawBitmap, r.left, r.top, null);
                    }
                }
            }
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            Log.d(TAG, "onSizeChanged pixels x=" + w + " pixels y=" + h);
            Log.v(TAG, "onSizeChanged density=" + Navit.sMetrics.density);
            Log.v(TAG, "onSizeChanged scaledDensity=" + Navit.sMetrics.scaledDensity);
            super.onSizeChanged(w, h, oldw, oldh);
            mDrawBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            mDrawCanvas = new Canvas(mDrawBitmap);
            mBitmapWidth = w;
            mBitmapHeight = h;
            sizeChangedCallback(mSizeChangedCallbackID, w, h);
        }

        void doLongpressAction() {
            Log.e(TAG, "doLongpressAction enter");
            mActivity.openContextMenu(this);
        }

        @Override
        public boolean onDown(MotionEvent motionEvent) {
            Log.e(TAG,"onDown");
            // if (sInMap) {
            buttonCallback(mButtonCallbackID, 1, 1,
                    (int)mPressedPosition.x, (int)mPressedPosition.y); // down
            //}
            return true;
        }

        public boolean onContextClick(MotionEvent motionEvent) {
            Log.e(TAG,"onContextClick: " + motionEvent.toString());
            return false;
        }

        @Override
        public void onShowPress(MotionEvent motionEvent) {
            Log.e(TAG,"onShowPress: " + motionEvent.toString());
        }

        @Override
        public boolean onSingleTapUp(MotionEvent motionEvent) {
            Log.e(TAG, "onSingleTapUp: " + motionEvent.toString());
            //  if (sInMap) {

                buttonCallback(mButtonCallbackID, 0, 1,
                        (int)mPressedPosition.x, (int)mPressedPosition.y); // up
            //  }
            return true;
        }

        //@Override
        public boolean onSingleTapConfirmed(MotionEvent motionEvent) {
            Log.e(TAG, "onSingleTapConfirmed: " + motionEvent.toString());
            return false;
        }

        @Override
        public boolean onScroll(MotionEvent event1, MotionEvent event2, float distanceX,
                                float distanceY) {
            Log.w(TAG, "onScroll: " + event1.toString() + event2.toString());
            motionCallback(mMotionCallbackID, (int)event2.getX(), (int)event2.getY());
            return false;
        }

        @Override
        public void onLongPress(MotionEvent motionEvent) {
            Log.e(TAG, "onLongPress: " + motionEvent.toString());
            if (sInMap) {
                doLongpressAction();
            }
        }

        public boolean onFling(MotionEvent event1, MotionEvent event2,
                               float velocityX, float velocityY) {
            //Log.e(TAG, "onFling: " + event1.toString() + event2.toString());
            Log.e(TAG, "onFling: " + velocityX + " " + velocityY);
            return false;
            //FlingAnimation fling = new FlingAnimation(this, DynamicAnimation.SCROLL_X);
            //fling.setStartVelocity(-velocityX)
            //        .setMinValue(0)
            //        .setMaxValue(600) //maxscroll
            //        .setFriction(1.1f)
            //        .start();

            //    Log.e(TAG,"scrolling ENDED");
            //    return true;
        }

        public boolean performClick() {
            super.performClick();
            Log.e(TAG,"performClick");
            return true; //??????????? of moet dat return super.... zijn ?
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {

            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                int x = (int) event.getX();
                int y = (int) event.getY();
                mPressedPosition = new PointF(x, y);
                Log.e(TAG,"ACTION_DOWN point " + mPressedPosition.toString());
            //    performClick();
            }
            if (this.mDetector.onTouchEvent(event)){
                return true;
            }
            if (event.getAction() == MotionEvent.ACTION_UP) { //afblokken als er een fling is en daarna dan afhandelen ?
                int x = (int) event.getX();
                int y = (int) event.getY();
                //    mPressedPosition = new PointF(x, y);
                Log.e(TAG,"ACTION_UP point " + mPressedPosition.toString());
                buttonCallback(mButtonCallbackID, 0, 1, x, y); // up
                //    performClick();
            }

            //  return  super.onTouchEvent(event); // if not processed, give super a chance
            return true; // anders krijg je de rest niet meer
        }



            //  super.onTouchEvent(event); -----------------------!!!!!!
        //    int x = (int) event.getX();
        //    int y = (int) event.getY();
        //    int switchValue = (event.getActionMasked());
            //Log.e(TAG, "onTouchEvent value =  " + switchValue);

         //   if (switchValue == MotionEvent.ACTION_DOWN) {
         //       mTouchMode = PRESSED;
         //       Log.d(TAG, "ACTION_DOWN mode PRESSED");
         //       if (!sInMap) {
         //           buttonCallback(mButtonCallbackID, 1, 1, x, y); // down
         //       }
         //       mPressedPosition = new PointF(x, y);
                //postDelayed(this, TIME_FOR_LONG_PRESS);

         //   } else if (switchValue == MotionEvent.ACTION_POINTER_DOWN) {
         //       mOldDist = spacing(event);
         //       if (mOldDist > 2f) {
         //           mTouchMode = ZOOM;
         //           Log.d(TAG, "ACTION_DOWN mode ZOOM started");
         //       }
         //   } else if (switchValue == MotionEvent.ACTION_UP) {
         //       Log.d(TAG, "ACTION_UP");
         //       switch (mTouchMode) {
         //           case DRAG:
         //               Log.d(TAG, "onTouch move");
         //               motionCallback(mMotionCallbackID, x, y);
         //               buttonCallback(mButtonCallbackID, 0, 1, x, y); // up
         //               break;
         //           case PRESSED:
         //               if (sInMap) {
         //                   buttonCallback(mButtonCallbackID, 1, 1, x, y); // down
         //               }
         //               buttonCallback(mButtonCallbackID, 0, 1, x, y); // up
         //               break;
         //           default:
         //               Log.i(TAG, "Unexpected touchmode: " + mTouchMode);
         //       }
         //       mTouchMode = NONE;
         //   } else if (switchValue == MotionEvent.ACTION_MOVE) {
         //       switch (mTouchMode) {
         //           case DRAG:
         //               motionCallback(mMotionCallbackID, x, y);
         //               break;
         //           case ZOOM:
         //               doZoom(event);
         //               break;
         //           case PRESSED:
         //               Log.d(TAG, "Start drag mode");
         //               if (spacing(mPressedPosition, new PointF(event.getX(), event.getY())) > 20f) {
         //                   buttonCallback(mButtonCallbackID, 1, 1, x, y); // down
         //                   mTouchMode = DRAG;
         //               }
         //               break;
         //           default:
         //               Log.i(TAG, "Unexpected touchmode: " + mTouchMode);
         //       }
         //   }
         //   return true;
        //}

        // unused
        private void doZoom(MotionEvent event) {
            if (event.findPointerIndex(0) == -1 || event.findPointerIndex(1) == -1) {
                Log.e(TAG,"missing pointer");
                return;
            }
            float newDist = spacing(event);
            float scale;
            if (event.getActionMasked() == MotionEvent.ACTION_MOVE) {
                scale = newDist / mOldDist;
                Log.v(TAG, "New scale = " + scale);
                if (scale > 1.2) {
                    // zoom in
                    CallBackHandler.sendCommand(CallBackHandler.CmdType.CMD_ZOOM_IN);
                    mOldDist = newDist;
                } else if (scale < 0.8) {
                    mOldDist = newDist;
                    // zoom out
                    CallBackHandler.sendCommand(CallBackHandler.CmdType.CMD_ZOOM_OUT);
                }
            }
        }

        private float spacing(MotionEvent event) {
            float x = event.getX(0) - event.getX(1);
            float y = event.getY(0) - event.getY(1);
            return (float) Math.sqrt(x * x + y * y);
        }

        private float spacing(PointF a, PointF b) {
            float x = a.x - b.x;
            float y = a.y - b.y;
            return (float)Math.sqrt(x * x + y * y);
        }

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            Log.d(TAG,"onkeydown = " + keyCode);
            String keyStr = null;
            switch (keyCode) {
                case KeyEvent.KEYCODE_ENTER:
                case KeyEvent.KEYCODE_DPAD_CENTER:
                    keyStr = String.valueOf((char) 13);
                    break;
                case KeyEvent.KEYCODE_DEL:
                    keyStr = String.valueOf((char) 8);
                    break;
                //case KeyEvent.KEYCODE_MENU:
                //    if (!sInMap) {
                //        this.postInvalidate();
                //        return true;
                //    }
                //    break;
                case KeyEvent.KEYCODE_SEARCH:
                    /* Handle event in Main Activity if map is shown */
                    if (!sInMap) {
                        keyStr = String.valueOf((char) 19);
                    }
                    break;
                case KeyEvent.KEYCODE_BACK:
                    keyStr = String.valueOf((char) 27);
                    break;
                case KeyEvent.KEYCODE_CALL:
                    keyStr = String.valueOf((char) 3);
                    break;
                case KeyEvent.KEYCODE_DPAD_DOWN:
                    keyStr = String.valueOf((char) 14);
                    break;
                case KeyEvent.KEYCODE_DPAD_LEFT:
                    keyStr = String.valueOf((char) 2);
                    break;
                case KeyEvent.KEYCODE_DPAD_RIGHT:
                    keyStr = String.valueOf((char) 6);
                    break;
                case KeyEvent.KEYCODE_DPAD_UP:
                    keyStr = String.valueOf((char) 16);
                    break;
                default:
                    Log.e(TAG, "keycode: " + keyCode);
            }
            if (keyStr != null) {
                keypressCallback(mKeypressCallbackID, keyStr);
                return true;
            }
            return false;
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            Log.d(TAG,"onkeyUp = " + keyCode);
            int i;
            String s = null;
            i = event.getUnicodeChar();

            if (i == 0) {
                switch (keyCode) {
                    case KeyEvent.KEYCODE_VOLUME_UP:
                    case KeyEvent.KEYCODE_VOLUME_DOWN:
                        return (!sInMap);
                    case KeyEvent.KEYCODE_SEARCH:
                        /* Handle event in Main Activity if map is shown */
                        if (sInMap) {
                            return false;
                        }
                        break;
                    case KeyEvent.KEYCODE_BACK:
                        Navit.sShowSoftKeyboardShowing = false;
                        return true;
                    case KeyEvent.KEYCODE_MENU:
                        if (!sInMap) {
                            if (!Navit.sShowSoftKeyboardShowing) {
                                // if in menu view:
                                // use as OK (Enter) key
                                s = String.valueOf((char) 13);
                            } // if soft keyboard showing on screen, dont use menu button as select key
                        } else {
                            return false;
                        }
                        break;
                    default:
                        Log.v(TAG, "keycode: " + keyCode);
                }
            } else if (i != 10) {
                s = java.lang.String.valueOf((char) i);
            }
            if (s != null) {
                keypressCallback(mKeypressCallbackID, s);
            }
            return true;
        }

        @Override
        public boolean onKeyMultiple(int keyCode, int count, KeyEvent event) {
            String s;
            if (keyCode == KeyEvent.KEYCODE_UNKNOWN) {
                s = event.getCharacters();
                keypressCallback(mKeypressCallbackID, s);
                return true;
            }
            return super.onKeyMultiple(keyCode, count, event);
        }

        @Override
        public boolean onTrackballEvent(MotionEvent event) {
            String s = null;
            if (event.getAction() == android.view.MotionEvent.ACTION_DOWN) {
                s = java.lang.String.valueOf((char) 13);
            }
            if (event.getAction() == android.view.MotionEvent.ACTION_MOVE) {
                mTrackballX += event.getX();
                mTrackballY += event.getY();
                if (mTrackballX <= -1) {
                    s = java.lang.String.valueOf((char) 2);
                    mTrackballX += 1;
                }
                if (mTrackballX >= 1) {
                    s = java.lang.String.valueOf((char) 6);
                    mTrackballX -= 1;
                }
                if (mTrackballY <= -1) {
                    s = java.lang.String.valueOf((char) 16);
                    mTrackballY += 1;
                }
                if (mTrackballY >= 1) {
                    s = java.lang.String.valueOf((char) 14);
                    mTrackballY -= 1;
                }
            }
            if (s != null) {
                keypressCallback(mKeypressCallbackID, s);
            }
            return true;
        }
    }

    NavitGraphics(final Activity navit, NavitGraphics parent, int x, int y, int w, int h,
                         int wraparound, int useCamera) {
        if (parent == null) {
            if (useCamera != 0) {
                setCamera(useCamera);
            }
            mOverlays = new ArrayList<>();
            setmActivity((Navit)navit);
        } else {
            mOverlays = null;
            mDrawBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            mBitmapWidth = w;
            mBitmapHeight = h;
            mPosX = x;
            mPosY = y;
            mPosWraparound = wraparound;
            mDrawCanvas = new Canvas(mDrawBitmap);
            parent.mOverlays.add(this);
        }
        mParentGraphics = parent;
    }

    /**
     * Sets up the main view.
     * @param navit The main activity.
     */
    private void setmActivity(final Navit navit) {
        this.mActivity = navit;
        mView = new NavitView(mActivity);
        mView.setClickable(false);
        mView.setFocusable(true);
        mView.setFocusableInTouchMode(true);
        mView.setKeepScreenOn(true);
        mRelativeLayout = new RelativeLayout(mActivity);

        RelativeLayout.LayoutParams lpLeft = new RelativeLayout.LayoutParams(96,96);
        lpLeft.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
        mZoomOutButton = new ImageButton(mActivity);
        mZoomOutButton.setLayoutParams(lpLeft);
        mZoomOutButton.setBackgroundResource(R.drawable.zoom_out);
        mZoomOutButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                CallBackHandler.sendCommand(CallBackHandler.CmdType.CMD_ZOOM_OUT);
            }
        });

        mZoomInButton = new ImageButton(mActivity);
        RelativeLayout.LayoutParams lpRight = new RelativeLayout.LayoutParams(96,96);
        lpRight.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
        mZoomInButton.setLayoutParams(lpRight);
        mZoomInButton.setBackgroundResource(R.drawable.zoom_in);
        mZoomInButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                CallBackHandler.sendCommand(CallBackHandler.CmdType.CMD_ZOOM_IN);
            }
        });

        mRelativeLayout.addView(mView);
        mRelativeLayout.addView(mZoomInButton);
        mRelativeLayout.addView(mZoomOutButton);
        mActivity.setContentView(mRelativeLayout);
        mView.requestFocus();
    }


    private native void sizeChangedCallback(long id, int x, int y);

    private native void keypressCallback(long id, String s);

    private native void buttonCallback(long id, int pressed, int button, int x, int y);

    private native void motionCallback(long id, int x, int y);

    static native String[][] getAllCountries();

    private Canvas mDrawCanvas;
    private Bitmap mDrawBitmap;
    private long mSizeChangedCallbackID;
    private long mButtonCallbackID;
    private long mMotionCallbackID;
    private long mKeypressCallbackID;


    /**
     * Returns whether the device has a hardware menu button.
     *
     */
    boolean hasMenuButton() {
        return ViewConfiguration.get(mActivity.getApplication()).hasPermanentMenuKey();
    }

    void setSizeChangedCallback(long id) {
        mSizeChangedCallbackID = id;
    }

    void setButtonCallback(long id) {
        Log.v(TAG,"set Buttononcallback");
        mButtonCallbackID = id;
    }

    void setMotionCallback(long id) {
        mMotionCallbackID = id;
        Log.v(TAG,"set Motioncallback");
    }

    void setKeypressCallback(long id) {
        Log.v(TAG,"set Keypresscallback");
        mKeypressCallbackID = id;
    }


    protected void draw_polyline(Paint paint, int[] c) {
        paint.setStrokeWidth(c[0]);
        paint.setARGB(c[1],c[2],c[3],c[4]);
        paint.setStyle(Paint.Style.STROKE);
        //paint.setAntiAlias(true);
        //paint.setStrokeWidth(0);
        int ndashes = c[5];
        float[] intervals = new float[ndashes + (ndashes % 2)];
        for (int i = 0; i < ndashes; i++) {
            intervals[i] = c[6 + i];
        }

        if ((ndashes % 2) == 1) {
            intervals[ndashes] = intervals[ndashes - 1];
        }

        if (ndashes > 0) {
            paint.setPathEffect(new android.graphics.DashPathEffect(intervals,0.0f));
        }

        Path path = new Path();
        path.moveTo(c[6 + ndashes], c[7 + ndashes]);
        for (int i = 8 + ndashes; i < c.length; i += 2) {
            path.lineTo(c[i], c[i + 1]);
        }
        //global_path.close();
        mDrawCanvas.drawPath(path, paint);
        paint.setPathEffect(null);
    }

    protected void draw_polygon(Paint paint, int[] c) {
        paint.setStrokeWidth(c[0]);
        paint.setARGB(c[1],c[2],c[3],c[4]);
        paint.setStyle(Paint.Style.FILL);
        //paint.setAntiAlias(true);
        //paint.setStrokeWidth(0);
        Path path = new Path();
        path.moveTo(c[5], c[6]);
        for (int i = 7; i < c.length; i += 2) {
            path.lineTo(c[i], c[i + 1]);
        }
        //global_path.close();
        mDrawCanvas.drawPath(path, paint);
    }

    protected void draw_rectangle(Paint paint, int x, int y, int w, int h) {
        Rect r = new Rect(x, y, x + w, y + h);
        paint.setStyle(Paint.Style.FILL);
        paint.setAntiAlias(true);
        //paint.setStrokeWidth(0);
        mDrawCanvas.drawRect(r, paint);
    }

    protected void draw_circle(Paint paint, int x, int y, int r) {
        paint.setStyle(Paint.Style.STROKE);
        mDrawCanvas.drawCircle(x, y, r / 2, paint);
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
                mDrawCanvas.drawText(text, x, y, paint);
            } else {
                mDrawCanvas.drawTextOnPath(text, path, 0, 0, paint);
            }
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(oldcolor);
        }

        if (path == null) {
            mDrawCanvas.drawText(text, x, y, paint);
        } else {
            mDrawCanvas.drawTextOnPath(text, path, 0, 0, paint);
        }
        paint.clearShadowLayer();
    }

    protected void draw_image(Paint paint, int x, int y, Bitmap bitmap) {
        mDrawCanvas.drawBitmap(bitmap, x, y, null);
    }

    /* takes an image and draws it on the screen as a prerendered maptile
     *
     *
     *
     * @param imagepath	    a string representing an absolute or relative path
     *                      to the image file
     * @param count	        the number of points specified
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
        Log.e("NavitGraphics","path = " + imagepath);
        Log.e("NavitGraphics","count = " + count);

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
                imagepath = Navit.sMapFilenamePath + imagepath;
            }
            Log.e("NavitGraphics","pathc3 = " + imagepath);
            try {
                infile = new FileInputStream(imagepath);
                bitmap = BitmapFactory.decodeStream(infile);
                infile.close();
            } catch (IOException e) {
                Log.e("NavitGraphics","could not open " + imagepath);
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
                mDrawCanvas.drawBitmap(bitmap, matrix, null);
            }
        }
        //  }
        //  else
        //     ((NavitCanvas)draw_canvas).draw_image_warp(imagepath,count,p0x,p0y,p1x,p1y,p2x,p2y);

    }


    /* These constants must be synchronized with enum draw_mode_num in graphics.h. */
    private static final int DRAW_MODE_BEGIN = 0;
    private static final int DRAW_MODE_END = 1;
    /* Used by the pedestrian plugin, draws without a mapbackground */
    private static final int DRAW_MODE_BEGIN_CLEAR = 2;

    protected void draw_mode(int mode) {
        if (mode == DRAW_MODE_END) {
            if (mParentGraphics == null) {
                if (mOverlayDisabled == 0) {
                    this.setButtonState(true);
                } else {
                    this.setButtonState(false);
                }
                mView.invalidate();
            } else {
                mParentGraphics.mView.invalidate(get_rect());
            }
        }
        if (mode == DRAW_MODE_BEGIN_CLEAR || (mode == DRAW_MODE_BEGIN && mParentGraphics != null)) {
            mDrawBitmap.eraseColor(0);
        }
    }

    private void setButtonState(boolean b) {
        this.mZoomInButton.setEnabled(b);
        this.mZoomOutButton.setEnabled(b);
        if (b) {
            this.mZoomInButton.setVisibility(View.VISIBLE);
            this.mZoomOutButton.setVisibility(View.VISIBLE);
        } else {
            this.mZoomInButton.setVisibility(View.INVISIBLE);
            this.mZoomOutButton.setVisibility(View.INVISIBLE);
        }
        Log.d("Navitgraphics", "setbuttons " + b);
    }

    protected void draw_drag(int x, int y) {
        mPosX = x;
        mPosY = y;
    }

    protected void overlay_disable(int disable) {
        Log.v(TAG,"overlay_disable: " + disable + ", Parent: " + (mParentGraphics != null));
        if (mParentGraphics == null) {
            sInMap = (disable == 0);
        }
        if (mOverlayDisabled != disable) {
            mOverlayDisabled = disable;
            if (mParentGraphics != null) {
                mParentGraphics.mView.invalidate(get_rect());
            }
        }
    }

    protected void overlay_resize(int x, int y, int w, int h, int wraparound) {
        mDrawBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        mBitmapWidth = w;
        mBitmapHeight = h;
        mPosX = x;
        mPosY = y;
        mPosWraparound = wraparound;
        mDrawCanvas.setBitmap(mDrawBitmap);
    }
}
