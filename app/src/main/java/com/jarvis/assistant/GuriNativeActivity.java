package com.jarvis.assistant;

import android.app.NativeActivity;
import android.os.Bundle;
import android.util.Log;

/**
 * NativeActivity wrapper that records lifecycle and permission transitions.
 * The native C++ engine remains the application UI and command implementation.
 */
public final class GuriNativeActivity extends NativeActivity {
    private static final String TAG = "GURI_ACTIVITY";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "GURI Activity created");
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "GURI Activity resumed");
    }

    @Override
    protected void onPause() {
        Log.i(TAG, "GURI Activity paused");
        super.onPause();
    }

    @Override
    protected void onStop() {
        Log.i(TAG, "GURI Activity stopped");
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        Log.i(TAG, "GURI Activity destroyed finishing=" + isFinishing());
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        Log.i(TAG, "GURI window focus changed: " + hasFocus);
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        String permission = permissions != null && permissions.length > 0
                ? permissions[0] : "unknown";
        String result = grantResults != null && grantResults.length > 0
                ? (grantResults[0] == android.content.pm.PackageManager.PERMISSION_GRANTED
                    ? "granted" : "denied")
                : "no-result";
        Log.i(TAG, "Permission callback requestCode=" + requestCode
                + " permission=" + permission + " result=" + result);
    }
}
