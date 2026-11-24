#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/input.h>
#include <android/native_window.h>
#include <aaudio/AAudio.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <jni.h>
#include "command_engine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define LOG_TAG "JARVIS_NATIVE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr const char* kVertexShader =
    "attribute vec2 aPosition; void main(){ gl_Position=vec4(aPosition,0.0,1.0); }";
constexpr const char* kFragmentShader =
    "precision mediump float; uniform vec4 uColor; void main(){ gl_FragColor=uColor; }";

struct Engine {
    android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    GLuint program = 0;
    GLint positionLocation = -1;
    GLint colorLocation = -1;
    int width = 0;
    int height = 0;
    bool recording = false;
    std::string status = "NATIVE C++ CORE READY";
    AAudioStream* audioStream = nullptr;
    std::atomic<int> inputPeak{0};
};

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char message[512]{};
        glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
        LOGE("Shader compile error: %s", message);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool init_gl(Engine& e) {
    const EGLint attributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLint format = 0;
    EGLint count = 0;
    e.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (e.display == EGL_NO_DISPLAY || !eglInitialize(e.display, nullptr, nullptr)) return false;

    EGLConfig config;
    if (!eglChooseConfig(e.display, attributes, &config, 1, &count) || count == 0) return false;
    eglGetConfigAttrib(e.display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(e.app->window, 0, 0, format);

    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    e.context = eglCreateContext(e.display, config, EGL_NO_CONTEXT, contextAttributes);
    e.surface = eglCreateWindowSurface(e.display, config, e.app->window, nullptr);
    if (e.context == EGL_NO_CONTEXT || e.surface == EGL_NO_SURFACE) return false;
    if (!eglMakeCurrent(e.display, e.surface, e.surface, e.context)) return false;

    EGLint w = 0, h = 0;
    eglQuerySurface(e.display, e.surface, EGL_WIDTH, &w);
    eglQuerySurface(e.display, e.surface, EGL_HEIGHT, &h);
    e.width = w;
    e.height = h;

    GLuint vertex = compile_shader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vertex || !fragment) return false;
    e.program = glCreateProgram();
    glAttachShader(e.program, vertex);
    glAttachShader(e.program, fragment);
    glLinkProgram(e.program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(e.program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) return false;
    e.positionLocation = glGetAttribLocation(e.program, "aPosition");
    e.colorLocation = glGetUniformLocation(e.program, "uColor");
    return true;
}

void shutdown_gl(Engine& e) {
    if (e.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(e.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (e.program) glDeleteProgram(e.program);
        if (e.surface != EGL_NO_SURFACE) eglDestroySurface(e.display, e.surface);
        if (e.context != EGL_NO_CONTEXT) eglDestroyContext(e.display, e.context);
        eglTerminate(e.display);
    }
    e.display = EGL_NO_DISPLAY;
    e.surface = EGL_NO_SURFACE;
    e.context = EGL_NO_CONTEXT;
    e.program = 0;
}

void set_color(Engine& e, float r, float g, float b, float a = 1.0f) {
    glUniform4f(e.colorLocation, r, g, b, a);
}

void draw_rect(Engine& e, float x, float y, float w, float h,
               float r, float g, float b, float a = 1.0f) {
    const GLfloat vertices[] = {
        2.0f*x/e.width - 1.0f, 1.0f - 2.0f*y/e.height,
        2.0f*(x+w)/e.width - 1.0f, 1.0f - 2.0f*y/e.height,
        2.0f*x/e.width - 1.0f, 1.0f - 2.0f*(y+h)/e.height,
        2.0f*x/e.width - 1.0f, 1.0f - 2.0f*(y+h)/e.height,
        2.0f*(x+w)/e.width - 1.0f, 1.0f - 2.0f*y/e.height,
        2.0f*(x+w)/e.width - 1.0f, 1.0f - 2.0f*(y+h)/e.height
    };
    set_color(e, r, g, b, a);
    glVertexAttribPointer(e.positionLocation, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(e.positionLocation);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void draw_circle(Engine& e, float cx, float cy, float radius,
                 float r, float g, float b, float alpha = 1.0f) {
    constexpr int segments = 48;
    std::array<GLfloat, (segments + 2) * 2> vertices{};
    vertices[0] = 2.0f*cx/e.width - 1.0f;
    vertices[1] = 1.0f - 2.0f*cy/e.height;
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * 3.14159265f * static_cast<float>(i) / segments;
        const float px = cx + std::cos(angle) * radius;
        const float py = cy + std::sin(angle) * radius;
        vertices[(i+1)*2] = 2.0f*px/e.width - 1.0f;
        vertices[(i+1)*2+1] = 1.0f - 2.0f*py/e.height;
    }
    set_color(e, r, g, b, alpha);
    glVertexAttribPointer(e.positionLocation, 2, GL_FLOAT, GL_FALSE, 0, vertices.data());
    glEnableVertexAttribArray(e.positionLocation);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);
}

const char* glyph(char c) {
    switch (c) {
        case 'A': return "010101111101101"; case 'B': return "110101110101110";
        case 'C': return "011100100100011"; case 'D': return "110101101101110";
        case 'E': return "111100110100111"; case 'F': return "111100110100100";
        case 'G': return "011100101101011"; case 'H': return "101101111101101";
        case 'I': return "111010010010111"; case 'J': return "001001001101010";
        case 'K': return "101101110101101"; case 'L': return "100100100100111";
        case 'M': return "101111111101101"; case 'N': return "101111111111101";
        case 'O': return "010101101101010"; case 'P': return "110101110100100";
        case 'Q': return "010101101111011"; case 'R': return "110101110101101";
        case 'S': return "011100010001110"; case 'T': return "111010010010010";
        case 'U': return "101101101101111"; case 'V': return "101101101101010";
        case 'W': return "101101111111101"; case 'X': return "101101010101101";
        case 'Y': return "101101010010010"; case 'Z': return "111001010100111";
        case '0': return "111101101101111"; case '1': return "010110010010111";
        case '2': return "110001010100111"; case '3': return "110001010001110";
        case '4': return "101101111001001"; case '5': return "111100110001110";
        case '6': return "011100110101010"; case '7': return "111001010010010";
        case '8': return "010101010101010"; case '9': return "010101011001110";
        case '-': return "000000111000000"; case ':': return "000010000010000";
        case '.': return "000000000000010"; case '/': return "001001010100100";
        default: return "000000000000000";
    }
}

void draw_text(Engine& e, const std::string& value, float x, float y,
               float scale, float r, float g, float b) {
    float cursor = x;
    for (char raw : value) {
        const char c = raw >= 'a' && raw <= 'z' ? static_cast<char>(raw - 'a' + 'A') : raw;
        const char* bits = glyph(c);
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (bits[row*3 + col] == '1') {
                    draw_rect(e, cursor + col*scale, y + row*scale, scale*0.82f, scale*0.82f, r,g,b);
                }
            }
        }
        cursor += scale * 4.0f;
    }
}

void draw_ui(Engine& e) {
    if (e.width <= 0 || e.height <= 0 || e.display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(e.display, e.surface, e.surface, e.context);
    glViewport(0, 0, e.width, e.height);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(e.program);
    glClearColor(0.015f, 0.035f, 0.075f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_rect(e, 0, 0, e.width, e.height, 0.015f,0.035f,0.075f);
    draw_text(e, "JARVIS", e.width*0.08f, e.height*0.07f, 6.0f, 0.92f,0.97f,1.0f);
    draw_text(e, "NATIVE C++ ASSISTANT", e.width*0.08f, e.height*0.13f, 2.8f, 0.25f,0.75f,0.95f);

    const float cx = e.width * 0.5f;
    const float cy = e.height * 0.43f;
    const float radius = std::min(e.width, e.height) * 0.17f;
    draw_circle(e, cx, cy, radius*1.18f, 0.04f,0.40f,0.62f,0.22f);
    draw_circle(e, cx, cy, radius, 0.06f,0.33f,0.52f);
    draw_circle(e, cx, cy, radius*0.82f, 0.03f,0.10f,0.20f);
    draw_rect(e, cx-radius*0.10f, cy-radius*0.34f, radius*0.20f, radius*0.60f, 0.35f,0.88f,1.0f);
    draw_circle(e, cx, cy-radius*0.32f, radius*0.16f, 0.35f,0.88f,1.0f);
    draw_rect(e, cx-radius*0.33f, cy+radius*0.12f, radius*0.66f, radius*0.09f, 0.35f,0.88f,1.0f);

    const float labelScale = std::max(2.0f, std::min(4.2f, e.width/100.0f));
    const std::string prompt = e.recording ? "MICROPHONE ACTIVE" : "TAP TO SPEAK";
    draw_text(e, prompt, (e.width - prompt.size()*4.0f*labelScale)/2.0f,
              e.height*0.65f, labelScale, 0.38f,0.85f,1.0f);

    draw_text(e, e.status, e.width*0.07f, e.height*0.73f, 2.4f, 0.78f,0.86f,0.94f);

    const float margin = e.width*0.07f;
    const float gap = e.width*0.04f;
    const float buttonW = (e.width - 2*margin - gap)/2.0f;
    const float buttonY = e.height*0.82f;
    const float buttonH = std::max(52.0f, e.height*0.075f);
    draw_rect(e, margin, buttonY, buttonW, buttonH, 0.05f,0.20f,0.34f);
    draw_rect(e, margin+buttonW+gap, buttonY, buttonW, buttonH, 0.05f,0.20f,0.34f);
    draw_text(e, "WHATSAPP", margin+buttonW*0.23f, buttonY+buttonH*0.38f, labelScale, 0.55f,0.90f,1.0f);
    draw_text(e, "YOUTUBE", margin+buttonW+gap+buttonW*0.27f, buttonY+buttonH*0.38f, labelScale, 0.55f,0.90f,1.0f);

    const std::string footer = "C++ NDK - NO KOTLIN UI";
    draw_text(e, footer, (e.width-footer.size()*4.0f*2.0f)/2.0f, e.height*0.95f, 2.0f,0.30f,0.48f,0.60f);
    eglSwapBuffers(e.display, e.surface);
}

void clear_jni_exception(JNIEnv* env) {
    if (env->ExceptionCheck()) env->ExceptionClear();
}

bool has_audio_permission(Engine& e) {
    JNIEnv* env = e.app->activity->env;
    jobject activity = e.app->activity->clazz;
    jclass cls = env->GetObjectClass(activity);
    jmethodID check = env->GetMethodID(cls, "checkSelfPermission", "(Ljava/lang/String;)I");
    jstring permission = env->NewStringUTF("android.permission.RECORD_AUDIO");
    jint result = env->CallIntMethod(activity, check, permission);
    env->DeleteLocalRef(permission);
    env->DeleteLocalRef(cls);
    clear_jni_exception(env);
    return result == 0;
}

void request_audio_permission(Engine& e) {
    JNIEnv* env = e.app->activity->env;
    jobject activity = e.app->activity->clazz;
    jclass cls = env->GetObjectClass(activity);
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray permissions = env->NewObjectArray(1, stringClass, nullptr);
    jstring permission = env->NewStringUTF("android.permission.RECORD_AUDIO");
    env->SetObjectArrayElement(permissions, 0, permission);
    jmethodID request = env->GetMethodID(cls, "requestPermissions", "([Ljava/lang/String;I)V");
    env->CallVoidMethod(activity, request, permissions, 601);
    clear_jni_exception(env);
    env->DeleteLocalRef(permission);
    env->DeleteLocalRef(permissions);
    env->DeleteLocalRef(stringClass);
    env->DeleteLocalRef(cls);
}

aaudio_data_callback_result_t audio_callback(AAudioStream*, void* userData,
                                              void* audioData, int32_t numFrames) {
    auto* e = static_cast<Engine*>(userData);
    const auto* samples = static_cast<const int16_t*>(audioData);
    int peak = 0;
    for (int32_t i = 0; i < numFrames; ++i) {
        peak = std::max(peak, std::abs(static_cast<int>(samples[i])));
    }
    e->inputPeak.store(peak, std::memory_order_relaxed);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void stop_audio(Engine& e) {
    if (e.audioStream) {
        AAudioStream_requestStop(e.audioStream);
        AAudioStream_close(e.audioStream);
        e.audioStream = nullptr;
    }
    e.recording = false;
}

void start_audio(Engine& e) {
    if (!has_audio_permission(e)) {
        request_audio_permission(e);
        e.status = "ALLOW MIC PERMISSION THEN TAP AGAIN";
        return;
    }

    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) {
        e.status = "AUDIO INPUT UNAVAILABLE";
        return;
    }
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
    AAudioStreamBuilder_setSampleRate(builder, 16000);
    AAudioStreamBuilder_setChannelCount(builder, 1);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setDataCallback(builder, audio_callback, &e);

    const aaudio_result_t openResult = AAudioStreamBuilder_openStream(builder, &e.audioStream);
    AAudioStreamBuilder_delete(builder);
    if (openResult != AAUDIO_OK || !e.audioStream) {
        e.status = "FAILED TO OPEN MICROPHONE";
        e.audioStream = nullptr;
        return;
    }

    const aaudio_result_t startResult = AAudioStream_requestStart(e.audioStream);
    if (startResult != AAUDIO_OK) {
        stop_audio(e);
        e.status = "MICROPHONE START FAILED";
        return;
    }

    e.recording = true;
    e.status = "AUDIO CAPTURE READY - ASR NOT ADDED";
    LOGI("Native AAudio input started");
}

void launch_package(Engine& e, const char* packageName) {
    JNIEnv* env = e.app->activity->env;
    jobject activity = e.app->activity->clazz;
    jclass contextClass = env->GetObjectClass(activity);
    jmethodID getPackageManager = env->GetMethodID(
        contextClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject manager = env->CallObjectMethod(activity, getPackageManager);
    clear_jni_exception(env);

    jclass managerClass = env->GetObjectClass(manager);
    jmethodID getLaunchIntent = env->GetMethodID(
        managerClass, "getLaunchIntentForPackage", "(Ljava/lang/String;)Landroid/content/Intent;");
    jstring package = env->NewStringUTF(packageName);
    jobject intent = env->CallObjectMethod(manager, getLaunchIntent, package);
    clear_jni_exception(env);

    if (intent) {
        jmethodID startActivity = env->GetMethodID(
            contextClass, "startActivity", "(Landroid/content/Intent;)V");
        env->CallVoidMethod(activity, startActivity, intent);
        clear_jni_exception(env);
        e.status = std::string("OPENED ") + packageName;
        env->DeleteLocalRef(intent);
    } else {
        e.status = std::string("APP NOT INSTALLED: ") + packageName;
    }

    env->DeleteLocalRef(package);
    env->DeleteLocalRef(managerClass);
    env->DeleteLocalRef(manager);
    env->DeleteLocalRef(contextClass);
}

void change_volume(Engine& e, bool up) {
    JNIEnv* env = e.app->activity->env;
    jobject activity = e.app->activity->clazz;
    jclass cls = env->GetObjectClass(activity);
    jmethodID getSystemService = env->GetMethodID(
        cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring service = env->NewStringUTF("audio");
    jobject audio = env->CallObjectMethod(activity, getSystemService, service);
    clear_jni_exception(env);

    if (audio) {
        jclass audioClass = env->GetObjectClass(audio);
        jmethodID adjust = env->GetMethodID(audioClass, "adjustStreamVolume", "(III)V");
        env->CallVoidMethod(audio, adjust, 3, up ? 1 : -1, 1);
        clear_jni_exception(env);
        env->DeleteLocalRef(audioClass);
        env->DeleteLocalRef(audio);
        e.status = up ? "MEDIA VOLUME INCREASED" : "MEDIA VOLUME DECREASED";
    } else {
        e.status = "VOLUME CONTROL UNAVAILABLE";
    }

    env->DeleteLocalRef(service);
    env->DeleteLocalRef(cls);
}

void execute_command(Engine& e, jarvis::Command command) {
    switch (command) {
        case jarvis::Command::OpenWhatsApp:
            launch_package(e, "com.whatsapp");
            break;
        case jarvis::Command::OpenYouTube:
            launch_package(e, "com.google.android.youtube");
            break;
        case jarvis::Command::VolumeUp:
            change_volume(e, true);
            break;
        case jarvis::Command::VolumeDown:
            change_volume(e, false);
            break;
        case jarvis::Command::OpenDialer:
            e.status = "DIALER VOICE ACTION NOT YET CONNECTED";
            break;
        default:
            e.status = "COMMAND NOT RECOGNIZED";
            break;
    }
}

int32_t handle_input(android_app* app, AInputEvent* event) {
    auto* e = static_cast<Engine*>(app->userData);
    if (!e || AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
    if (AMotionEvent_getAction(event) != AMOTION_EVENT_ACTION_UP) return 1;

    const float x = AMotionEvent_getX(event, 0);
    const float y = AMotionEvent_getY(event, 0);
    const float cx = e->width * 0.5f;
    const float cy = e->height * 0.43f;
    const float radius = std::min(e->width, e->height) * 0.19f;

    if (std::hypot(x-cx, y-cy) <= radius) {
        if (e->recording) {
            stop_audio(*e);
            e->status = "MICROPHONE CAPTURE STOPPED - ASR PENDING";
        } else {
            start_audio(*e);
        }
        return 1;
    }

    const float buttonY = e->height*0.82f;
    if (y >= buttonY && y <= buttonY + std::max(52.0f, e->height*0.075f)) {
        if (x < e->width*0.5f) launch_package(*e, "com.whatsapp");
        else launch_package(*e, "com.google.android.youtube");
        return 1;
    }

    if (y > e->height*0.90f) {
        execute_command(*e, jarvis::classify_command("volume up"));
        return 1;
    }
    return 1;
}

void handle_cmd(android_app* app, int32_t cmd) {
    auto* e = static_cast<Engine*>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window && init_gl(*e)) draw_ui(*e);
            else e->status = "OPENGL INITIALIZATION FAILED";
            break;
        case APP_CMD_TERM_WINDOW:
            shutdown_gl(*e);
            break;
        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_WINDOW_RESIZED:
            if (e->display != EGL_NO_DISPLAY) draw_ui(*e);
            break;
        case APP_CMD_LOST_FOCUS:
            break;
        default:
            break;
    }
}

} // namespace

void android_main(android_app* app) {
    Engine engine;
    engine.app = app;
    app->userData = &engine;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    while (true) {
        int events = 0;
        android_poll_source* source = nullptr;
        const int timeout = engine.display == EGL_NO_DISPLAY ? -1 : 50;
        int ident = ALOOPER_POLL_TIMEOUT;
        do {
            ident = ALooper_pollOnce(timeout, nullptr, &events,
                                     reinterpret_cast<void**>(&source));
            if (ident >= 0 && source) source->process(app, source);
            if (app->destroyRequested) {
                stop_audio(engine);
                shutdown_gl(engine);
                return;
            }
        } while (ident >= 0);
        if (engine.display != EGL_NO_DISPLAY) draw_ui(engine);
    }
}
