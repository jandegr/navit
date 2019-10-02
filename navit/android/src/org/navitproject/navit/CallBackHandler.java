package org.navitproject.navit;

import android.os.Handler;
import android.os.Message;
import android.util.Log;


class CallBackHandler extends Handler {

    private static final MsgType[] msg_values = MsgType.values();
    static final Handler sCallbackHandler = new CallBackHandler();
    private static final String TAG = "NavitCallB";


    private static native int callbackCmdChannel(int i);

    static void sendCommand(CmdType command) {
        switch (command) {
            case CMD_ZOOM_IN:
                callbackCmdChannel(1);
                break;
            case CMD_ZOOM_OUT:
                callbackCmdChannel(2);
                break;
            case CMD_BLOCK:
                callbackCmdChannel(1);
                break;
            case CMD_UNBLOCK:
                callbackCmdChannel(2);
                break;
            case CMD_REDRAW:
            default:
                Log.e(TAG, "Unhandled command : " + command);
        }
    }

    private static native int callbackMessageChannel(int i, String s);

    public void handleMessage(Message msg) {
        switch (msg_values[msg.what]) {
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
            case CLB_CALL_CMD: // call a command (like in gui)
                String cmd = msg.getData().getString(("cmd"));
                callbackMessageChannel(5, cmd);
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

    enum CmdType { CMD_ZOOM_IN, CMD_ZOOM_OUT, CMD_BLOCK, CMD_UNBLOCK, CMD_REDRAW }

    enum MsgType { CLB_REDRAW, CLB_MOVE, CLB_SET_DESTINATION, CLB_SET_DISPLAY_DESTINATION,
        CLB_CALL_CMD, CLB_LOAD_MAP, CLB_UNLOAD_MAP, CLB_DELETE_MAP, CLB_ABORT_NAVIGATION }
}
