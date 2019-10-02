package org.navitproject.navit;

import android.os.Handler;
import android.os.Message;
import android.util.Log;


class CallBackHandler extends Handler {

    private static final MsgType[] msg_values = MsgType.values();
    static final Handler sCallbackHandler = new CallBackHandler();
    private static final String TAG = "NavitCallB";

    // er zijn er nog een paar die dit rechtstreeks aanroepen,
    // na te zien
    static native int callbackMessageChannel(int i, String s);

    public void handleMessage(Message msg) {
        switch (msg_values[msg.what]) {
            case CLB_ZOOM_IN:
                callbackMessageChannel(1, "");
                break;
            case CLB_ZOOM_OUT:
                callbackMessageChannel(2, "");
                break;
            case CLB_MOVE:
                //motionCallback(mMotionCallbackID, msg.getData().getInt("x"), msg.getData().getInt("y"));
                break;
            case CLB_SET_DESTINATION:
                String lat = Float.toString(msg.getData().getFloat("lat"));
                String lon = Float.toString(msg.getData().getFloat("lon"));
                String q = msg.getData().getString(("q"));
                callbackMessageChannel(3, lat + "#" + lon + "#" + q);
                break;
            case CLB_SET_DISPLAY_DESTINATION:
                int x = msg.arg1;
                int y = msg.arg2;
                callbackMessageChannel(4, "" + x + "#" + y);
                break;
            case CLB_CALL_CMD:
                String cmd = msg.getData().getString(("cmd"));
                callbackMessageChannel(5, cmd);
                break;
            case CLB_BUTTON_UP:
                //buttonCallback(mButtonCallbackID, 0, 1, msg.getData().getInt("x"), msg.getData().getInt("y"));
                break;
            case CLB_BUTTON_DOWN:
                //buttonCallback(mButtonCallbackID, 1, 1, msg.getData().getInt("x"), msg.getData().getInt("y"));
                break;
            case CLB_COUNTRY_CHOOSER:
                break;
            case CLB_LOAD_MAP:
                callbackMessageChannel(6, msg.getData().getString(("title")));
                break;
            case CLB_DELETE_MAP:
                //unload map before deleting it !!!
                callbackMessageChannel(7, msg.getData().getString(("title")));
                NavitUtils.removeFileIfExists(msg.getData().getString(("title")));
                break;
            case CLB_UNLOAD_MAP:
                callbackMessageChannel(7, msg.getData().getString(("title")));
                break;
            case CLB_REDRAW:
            default:
                Log.e(TAG, "Unhandled callback : " + msg_values[msg.what]);
        }
    }

    enum MsgType {
        CLB_ZOOM_IN, CLB_ZOOM_OUT, CLB_REDRAW, CLB_MOVE, CLB_BUTTON_UP, CLB_BUTTON_DOWN, CLB_SET_DESTINATION
        , CLB_SET_DISPLAY_DESTINATION, CLB_CALL_CMD, CLB_COUNTRY_CHOOSER, CLB_LOAD_MAP, CLB_UNLOAD_MAP, CLB_DELETE_MAP
        ,CLB_ABORT_NAVIGATION, CLB_BLOCK, CLB_UNBLOCK
    }
}
