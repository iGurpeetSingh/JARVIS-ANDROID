package com.jarvis.assistant;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.SweepGradient;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;

public final class JarvisVoiceService extends Service {
    private static final String CHANNEL_ID = "guri_voice_listener";
    private static final int NOTIFICATION_ID = 7401;
    private static final String ACTION_STOP = "com.jarvis.assistant.action.STOP_VOICE_SERVICE";
    private static volatile JarvisVoiceService instance;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private EdgeOverlay edgeOverlay;

    static {
        try {
            System.loadLibrary("jarvis_native_app");
        } catch (UnsatisfiedLinkError error) {
            android.util.Log.e("GURI_SERVICE", "Native engine library is unavailable", error);
        }
    }

    private static native void nativeServiceStateChanged(boolean running);

    public static void setEdgeVisible(final boolean visible) {
        final JarvisVoiceService service = instance;
        if (service == null) {
            android.util.Log.w("GURI_SERVICE",
                    "RGB edge request received before voice service initialization");
            return;
        }
        service.mainHandler.post(() -> {
            if (service.edgeOverlay == null) return;
            if (visible) service.edgeOverlay.show();
            else service.edgeOverlay.hide();
        });
    }

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();

        // A rejected microphone foreground-service start must not crash the
        // NativeActivity process. Keep the failure visible in Logcat instead.
        try {
            startVoiceForeground();
        } catch (RuntimeException error) {
            android.util.Log.e("GURI_SERVICE",
                    "Microphone foreground service could not start; check RECORD_AUDIO and Android FGS rules",
                    error);
            stopSelf();
            return;
        }

        instance = this;
        edgeOverlay = new EdgeOverlay();
        try {
            nativeServiceStateChanged(true);
            android.util.Log.i("GURI_SERVICE", "GURI foreground voice service is active");
        } catch (UnsatisfiedLinkError error) {
            android.util.Log.e("GURI_SERVICE", "Cannot notify native voice engine", error);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && ACTION_STOP.equals(intent.getAction())) {
            stopSelf();
            return START_NOT_STICKY;
        }
        // Android may recreate the service after reclaiming its process. The
        // NativeActivity remains the owner of the native audio engine.
        return START_STICKY;
    }

    @Override public IBinder onBind(Intent intent) { return null; }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        stopSelf();
        super.onTaskRemoved(rootIntent);
    }

    @Override
    public void onDestroy() {
        if (edgeOverlay != null) edgeOverlay.hide();
        if (instance == this) instance = null;
        try {
            nativeServiceStateChanged(false);
        } catch (UnsatisfiedLinkError error) {
            android.util.Log.w("GURI_SERVICE", "Native engine already unloaded");
        }
        super.onDestroy();
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, "GURI voice listener", NotificationManager.IMPORTANCE_LOW);
        channel.setDescription("Keeps the offline GURI wake-word listener active while other apps are open.");
        NotificationManager manager = getSystemService(NotificationManager.class);
        if (manager != null) manager.createNotificationChannel(channel);
    }

    private void startVoiceForeground() {
        Intent stopIntent = new Intent(this, JarvisVoiceService.class);
        stopIntent.setAction(ACTION_STOP);
        PendingIntent stopPending = PendingIntent.getService(
                this, 7402, stopIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? new Notification.Builder(this, CHANNEL_ID)
                : new Notification.Builder(this);
        Notification notification = builder
                .setSmallIcon(android.R.drawable.ic_btn_speak_now)
                .setContentTitle("GURI background voice is active")
                .setContentText("Offline wake-word listening. RGB edge light appears during a command.")
                .setCategory(Notification.CATEGORY_SERVICE)
                .setOngoing(true)
                .addAction(new Notification.Action.Builder(
                        android.R.drawable.ic_media_pause, "Stop listening", stopPending).build())
                .build();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIFICATION_ID, notification,
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE);
        } else {
            startForeground(NOTIFICATION_ID, notification);
        }
    }

    private final class EdgeOverlay {
        private final WindowManager windowManager =
                (WindowManager) getSystemService(WINDOW_SERVICE);
        private EdgeLightView view;

        void show() {
            if (view != null) return;
            if (windowManager == null) {
                android.util.Log.e("GURI_SERVICE", "WindowManager is unavailable; RGB edge light cannot be displayed");
                return;
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M &&
                    !Settings.canDrawOverlays(JarvisVoiceService.this)) {
                android.util.Log.w("GURI_SERVICE", "Overlay permission is disabled; RGB edge light was not displayed");
                return;
            }
            EdgeLightView next = new EdgeLightView(JarvisVoiceService.this);
            int flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                    | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                    | WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED;
            WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                    WindowManager.LayoutParams.MATCH_PARENT,
                    WindowManager.LayoutParams.MATCH_PARENT,
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                            ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                            : WindowManager.LayoutParams.TYPE_PHONE,
                    flags, PixelFormat.TRANSLUCENT);
            params.gravity = Gravity.TOP | Gravity.START;
            params.setTitle("GURI RGB Edge Light");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                params.layoutInDisplayCutoutMode =
                        WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            }
            try {
                windowManager.addView(next, params);
                view = next;
                android.util.Log.i("GURI_SERVICE", "RGB edge light overlay attached");
            } catch (WindowManager.BadTokenException | SecurityException error) {
                android.util.Log.e("GURI_SERVICE", "Could not show RGB edge overlay", error);
            }
        }

        void hide() {
            if (view == null || windowManager == null) return;
            try {
                windowManager.removeView(view);
            } catch (IllegalArgumentException ignored) {
                // The system may already have removed the overlay window.
            }
            view = null;
            android.util.Log.i("GURI_SERVICE", "RGB edge light overlay detached");
        }
    }

    private static final class EdgeLightView extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Matrix shaderMatrix = new Matrix();
        private final float density;
        private SweepGradient gradient;
        private float rotation;

        EdgeLightView(JarvisVoiceService context) {
            super(context);
            density = getResources().getDisplayMetrics().density;
            setBackground(new ColorDrawable(Color.TRANSPARENT));
            setLayerType(View.LAYER_TYPE_HARDWARE, null);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeCap(Paint.Cap.ROUND);
            paint.setStrokeJoin(Paint.Join.ROUND);
        }

        private float dp(float value) { return value * density; }

        @Override
        protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
            super.onSizeChanged(width, height, oldWidth, oldHeight);
            if (width <= 0 || height <= 0) return;
            int[] colors = {Color.CYAN, 0xff287bff, 0xff9b35ff, 0xffff35b8,
                    0xffff7138, 0xffffdf39, 0xff48ed9a, Color.CYAN};
            float[] positions = {0f, .14f, .29f, .43f, .57f, .71f, .86f, 1f};
            gradient = new SweepGradient(width / 2f, height / 2f, colors, positions);
            paint.setShader(gradient);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            if (gradient == null) return;
            float cx = getWidth() / 2f;
            float cy = getHeight() / 2f;
            shaderMatrix.setRotate(rotation, cx, cy);
            gradient.setLocalMatrix(shaderMatrix);
            float inset = dp(8f);
            RectF frame = new RectF(inset, inset, getWidth() - inset, getHeight() - inset);
            float radius = dp(30f);
            paint.setStrokeWidth(dp(10f));
            paint.setAlpha(70);
            canvas.drawRoundRect(frame, radius, radius, paint);
            paint.setStrokeWidth(dp(3.2f));
            paint.setAlpha(255);
            canvas.drawRoundRect(frame, radius, radius, paint);
            rotation = (rotation + 2.4f) % 360f;
            postInvalidateOnAnimation();
        }
    }
}
