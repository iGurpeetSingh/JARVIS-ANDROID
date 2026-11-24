package com.jarvis.assistant;

import android.app.Activity;
import android.content.pm.PackageManager;

/**
 * Runtime-permission requests initiated by the NativeActivity native thread
 * must be posted to Android's main/UI thread.
 */
public final class NativePermissionBridge {
    private NativePermissionBridge() {}

    public static void requestPermission(
            final Activity activity,
            final String permission,
            final int requestCode) {
        if (activity == null || permission == null || permission.isEmpty()) return;

        activity.runOnUiThread(() -> {
            if (activity.isFinishing() || activity.isDestroyed()) return;
            if (activity.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED) return;

            activity.requestPermissions(new String[] { permission }, requestCode);
        });
    }
}
