package com.jarvis.assistant;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.util.Log;

/**
 * Runtime-permission requests initiated by the NativeActivity native thread
 * must be posted to Android's main/UI thread.
 */
public final class NativePermissionBridge {
    private static final String TAG = "GURI_PERMISSION";

    private NativePermissionBridge() {}

    public static void requestPermission(
            final Activity activity,
            final String permission,
            final int requestCode) {
        if (activity == null || permission == null || permission.isEmpty()) {
            Log.e(TAG, "Permission request skipped: missing Activity or permission name");
            return;
        }

        Log.i(TAG, "Posting permission request to Android UI thread: " + permission);
        activity.runOnUiThread(() -> {
            if (activity.isFinishing() || activity.isDestroyed()) {
                Log.w(TAG, "Permission request skipped because GURI Activity is finishing");
                return;
            }
            if (activity.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED) {
                Log.i(TAG, "Permission is already granted: " + permission);
                return;
            }

            Log.i(TAG, "Showing Android runtime permission dialog: " + permission);
            activity.requestPermissions(new String[] { permission }, requestCode);
        });
    }
}
