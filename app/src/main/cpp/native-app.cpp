#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/input.h>
#include <android/native_window.h>
#include <aaudio/AAudio.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <jni.h>
#include "command_engine.h"
#include "offline-llm.h"
#include "sherpa-onnx/c-api/cxx-api.h"
#include "cxx-api.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <deque>
#include <thread>
#include <fstream>
#include <functional>
#include <utility>
#include <sys/stat.h>
#include <sys/types.h>

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
    std::atomic<bool> recording{false};
    std::string status = "NATIVE C++ CORE READY";
    AAudioStream* audioStream = nullptr;
    std::atomic<int> inputPeak{0};
    std::mutex audioMutex;
    std::vector<float> recordedSamples;
    std::mutex kwsMutex;
    std::condition_variable kwsCondition;
    std::deque<float> kwsAudioQueue;
    std::thread kwsThread;
    std::thread asrThread;
    std::thread ttsThread;
    std::thread llmThread;
    std::thread modelDownloadThread;
    std::unique_ptr<sherpa_onnx::cxx::OfflineTts> hindiTts;
    std::atomic<bool> speaking{false};
    std::atomic<bool> llmProcessing{false};
    std::atomic<bool> shuttingDown{false};
    std::mutex ttsMutex;
    std::atomic<bool> modelDownloading{false};
    std::atomic<bool> localModelReady{false};
    std::atomic<bool> pendingLlmQuery{false};
    std::string pendingQuestion;
    std::mutex statusMutex;
    std::atomic<int> pendingCommand{0};
    std::string pendingContact;
    std::string pendingYoutubeQuery;
    std::atomic<bool> processing{false};
    std::atomic<bool> wakeListening{false};
    std::atomic<bool> autoWakePending{false};
    std::atomic<bool> micPermissionPending{false};
    std::atomic<bool> wakeDetected{false};
    std::atomic<bool> commandReady{false};
    std::atomic<bool> speechStarted{false};
    std::atomic<bool> restartWake{false};
    std::atomic<bool> kwsFailed{false};
    std::atomic<int> silenceFrames{0};
    std::atomic<int> commandFrames{0};
    int32_t capturedSampleRate = 16000;
};

void set_status(Engine& e, const std::string& value) {
    std::lock_guard<std::mutex> lock(e.statusMutex);
    e.status = value;
}

std::string get_status(Engine& e) {
    std::lock_guard<std::mutex> lock(e.statusMutex);
    return e.status;
}

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
    const std::string prompt = e.processing.load() ? "PROCESSING COMMAND" :
        (e.wakeDetected.load() ? "SPEAK YOUR COMMAND" :
        (e.wakeListening.load() ? "SAY JARVIS" : "TAP TO LISTEN"));
    draw_text(e, prompt, (e.width - prompt.size()*4.0f*labelScale)/2.0f,
              e.height*0.65f, labelScale, 0.38f,0.85f,1.0f);

    draw_text(e, get_status(e), e.width*0.07f, e.height*0.73f, 2.4f, 0.78f,0.86f,0.94f);

    const float margin = e.width*0.07f;
    const float gap = e.width*0.04f;
    const float buttonW = (e.width - 2*margin - gap)/2.0f;
    const float buttonY = e.height*0.82f;
    const float buttonH = std::max(52.0f, e.height*0.075f);
    draw_rect(e, margin, buttonY, buttonW, buttonH, 0.05f,0.20f,0.34f);
    draw_rect(e, margin+buttonW+gap, buttonY, buttonW, buttonH, 0.05f,0.20f,0.34f);
    draw_text(e, "WHATSAPP", margin+buttonW*0.23f, buttonY+buttonH*0.38f, labelScale, 0.55f,0.90f,1.0f);
    draw_text(e, "YOUTUBE", margin+buttonW+gap+buttonW*0.27f, buttonY+buttonH*0.38f, labelScale, 0.55f,0.90f,1.0f);

    const std::string footer = "TAP HERE TO INSTALL OFFLINE AI";
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


bool has_contacts_permission(Engine& e) {
    JNIEnv* env = e.app->activity->env;
    jobject activity = e.app->activity->clazz;
    jclass cls = env->GetObjectClass(activity);
    jmethodID check = env->GetMethodID(cls, "checkSelfPermission", "(Ljava/lang/String;)I");
    jstring permission = env->NewStringUTF("android.permission.READ_CONTACTS");
    jint result = env->CallIntMethod(activity, check, permission);
    clear_jni_exception(env);
    env->DeleteLocalRef(permission);
    env->DeleteLocalRef(cls);
    return result == 0;
}

void request_contacts_permission(Engine& e) {
    JNIEnv* env = e.app->activity->env;
    jobject activity = e.app->activity->clazz;
    jclass cls = env->GetObjectClass(activity);
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray permissions = env->NewObjectArray(1, stringClass, nullptr);
    jstring permission = env->NewStringUTF("android.permission.READ_CONTACTS");
    env->SetObjectArrayElement(permissions, 0, permission);
    jmethodID request = env->GetMethodID(cls, "requestPermissions", "([Ljava/lang/String;I)V");
    env->CallVoidMethod(activity, request, permissions, 602);
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
    double squareSum = 0.0;

    if (e->wakeListening.load(std::memory_order_relaxed)) {
        {
            std::lock_guard<std::mutex> lock(e->kwsMutex);
            const size_t queueLimit = static_cast<size_t>(e->capturedSampleRate) * 4;
            for (int32_t i = 0; i < numFrames; ++i) {
                const float normalized = static_cast<float>(samples[i]) / 32768.0f;
                if (e->kwsAudioQueue.size() < queueLimit) e->kwsAudioQueue.push_back(normalized);
            }
        }
        e->kwsCondition.notify_one();
    }

    if (e->wakeDetected.load(std::memory_order_relaxed) &&
        !e->commandReady.load(std::memory_order_relaxed)) {
        {
            std::lock_guard<std::mutex> lock(e->audioMutex);
            const size_t maxSamples = static_cast<size_t>(e->capturedSampleRate) * 12;
            for (int32_t i = 0; i < numFrames; ++i) {
                const int value = static_cast<int>(samples[i]);
                peak = std::max(peak, std::abs(value));
                const float normalized = static_cast<float>(value) / 32768.0f;
                squareSum += static_cast<double>(normalized) * normalized;
                if (e->recordedSamples.size() < maxSamples) e->recordedSamples.push_back(normalized);
            }
        }

        e->commandFrames.fetch_add(numFrames, std::memory_order_relaxed);
        const double rms = std::sqrt(squareSum / std::max(1, numFrames));
        if (rms > 0.012) {
            e->speechStarted.store(true, std::memory_order_relaxed);
            e->silenceFrames.store(0, std::memory_order_relaxed);
        } else if (e->speechStarted.load(std::memory_order_relaxed)) {
            const int silent = e->silenceFrames.fetch_add(numFrames, std::memory_order_relaxed) + numFrames;
            if (silent >= e->capturedSampleRate * 9 / 10) e->commandReady.store(true, std::memory_order_release);
        }

        if (e->commandFrames.load(std::memory_order_relaxed) >= e->capturedSampleRate * 10) {
            e->commandReady.store(true, std::memory_order_release);
        }
    }

    e->inputPeak.store(peak, std::memory_order_relaxed);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void stop_audio(Engine& e) {
    e.recording.store(false);
    e.wakeListening.store(false);
    e.kwsCondition.notify_all();

    if (e.audioStream) {
        AAudioStream_requestStop(e.audioStream);
        AAudioStream_close(e.audioStream);
        e.audioStream = nullptr;
    }
    if (e.kwsThread.joinable() && e.kwsThread.get_id() != std::this_thread::get_id()) {
        e.kwsThread.join();
    }
}

bool copy_asset_to_file(AAssetManager* manager, const char* assetName,
                        const std::string& outputPath) {
    struct stat info {};
    if (stat(outputPath.c_str(), &info) == 0 && info.st_size > 0) return true;

    AAsset* asset = AAssetManager_open(manager, assetName, AASSET_MODE_STREAMING);
    if (!asset) {
        LOGE("Model asset not found: %s", assetName);
        return false;
    }

    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        AAsset_close(asset);
        LOGE("Cannot create model file: %s", outputPath.c_str());
        return false;
    }

    std::array<char, 1024 * 1024> buffer{};
    int count = 0;
    while ((count = AAsset_read(asset, buffer.data(), buffer.size())) > 0) {
        output.write(buffer.data(), count);
        if (!output) {
            output.close();
            AAsset_close(asset);
            return false;
        }
    }
    output.close();
    AAsset_close(asset);
    return count == 0;
}

bool copy_asset_tree(AAssetManager* manager, const std::string& assetPath,
                     const std::string& outputPath) {
    if (copy_asset_to_file(manager, assetPath.c_str(), outputPath)) return true;

    AAssetDir* directory = AAssetManager_openDir(manager, assetPath.c_str());
    if (!directory) return false;
    mkdir(outputPath.c_str(), 0700);

    bool ok = true;
    const char* name = nullptr;
    while ((name = AAssetDir_getNextFileName(directory)) != nullptr) {
        const std::string childAsset = assetPath + "/" + name;
        const std::string childOutput = outputPath + "/" + name;
        if (!copy_asset_tree(manager, childAsset, childOutput)) ok = false;
    }
    AAssetDir_close(directory);
    return ok;
}
void run_wake_word_engine(Engine& e) {
    const std::string modelDir = std::string(e.app->activity->internalDataPath) + "/jarvis-kws";
    mkdir(modelDir.c_str(), 0700);

    const std::string encoderPath = modelDir + "/encoder.int8.onnx";
    const std::string decoderPath = modelDir + "/decoder.onnx";
    const std::string joinerPath = modelDir + "/joiner.int8.onnx";
    const std::string tokensPath = modelDir + "/tokens.txt";
    const std::string keywordsPath = modelDir + "/keywords.txt";

    const bool ready =
        copy_asset_to_file(e.app->activity->assetManager, "jarvis-kws/encoder.int8.onnx", encoderPath) &&
        copy_asset_to_file(e.app->activity->assetManager, "jarvis-kws/decoder.onnx", decoderPath) &&
        copy_asset_to_file(e.app->activity->assetManager, "jarvis-kws/joiner.int8.onnx", joinerPath) &&
        copy_asset_to_file(e.app->activity->assetManager, "jarvis-kws/tokens.txt", tokensPath) &&
        copy_asset_to_file(e.app->activity->assetManager, "jarvis-kws/keywords.txt", keywordsPath);

    if (!ready) {
        set_status(e, "WAKE WORD MODEL ASSET ERROR");
        e.kwsFailed.store(true);
        e.wakeListening.store(false);
        return;
    }

    sherpa_onnx::cxx::KeywordSpotterConfig config;
    config.feat_config.sample_rate = 16000;
    config.feat_config.feature_dim = 80;
    config.model_config.transducer.encoder = encoderPath;
    config.model_config.transducer.decoder = decoderPath;
    config.model_config.transducer.joiner = joinerPath;
    config.model_config.tokens = tokensPath;
    config.model_config.model_type = "zipformer2";
    config.model_config.provider = "cpu";
    config.model_config.num_threads = 1;
    config.keywords_file = keywordsPath;
    config.keywords_score = 2.0f;
    config.keywords_threshold = 0.25f;
    config.max_active_paths = 4;

    set_status(e, "LOADING JARVIS WAKE WORD ENGINE");
    auto spotter = sherpa_onnx::cxx::KeywordSpotter::Create(config);
    if (!spotter.Get()) {
        set_status(e, "WAKE WORD MODEL FAILED TO LOAD");
        e.kwsFailed.store(true);
        e.wakeListening.store(false);
        return;
    }

    auto stream = spotter.CreateStream();
    std::vector<float> chunk;
    chunk.reserve(1600);
    set_status(e, "LISTENING FOR JARVIS");

    while (e.wakeListening.load()) {
        chunk.clear();
        {
            std::unique_lock<std::mutex> lock(e.kwsMutex);
            e.kwsCondition.wait(lock, [&e] {
                return !e.wakeListening.load() || e.kwsAudioQueue.size() >= 1600;
            });
            if (!e.wakeListening.load()) break;
            for (int i = 0; i < 1600; ++i) {
                chunk.push_back(e.kwsAudioQueue.front());
                e.kwsAudioQueue.pop_front();
            }
        }

        stream.AcceptWaveform(e.capturedSampleRate, chunk.data(), static_cast<int32_t>(chunk.size()));
        while (spotter.IsReady(&stream)) {
            spotter.Decode(&stream);
            const auto result = spotter.GetResult(&stream);
            if (!result.keyword.empty()) {
                LOGI("Wake phrase detected: %s", result.keyword.c_str());
                e.wakeDetected.store(true, std::memory_order_release);
                e.wakeListening.store(false);
                set_status(e, "JARVIS HEARD - SPEAK COMMAND");
                e.kwsCondition.notify_all();
                return;
            }
        }
    }
}

void play_generated_audio(const sherpa_onnx::cxx::GeneratedAudio& generated) {
    if (generated.samples.empty() || generated.sample_rate <= 0) return;

    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK || !builder) return;
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setChannelCount(builder, 1);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSampleRate(builder, AAUDIO_UNSPECIFIED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

    AAudioStream* output = nullptr;
    const aaudio_result_t opened = AAudioStreamBuilder_openStream(builder, &output);
    AAudioStreamBuilder_delete(builder);
    if (opened != AAUDIO_OK || !output) {
        LOGE("Hindi speech output stream could not be opened: %s", AAudio_convertResultToText(opened));
        return;
    }

    const int32_t targetRate = AAudioStream_getSampleRate(output);
    if (targetRate <= 0 || AAudioStream_requestStart(output) != AAUDIO_OK) {
        AAudioStream_close(output);
        return;
    }

    const double ratio = static_cast<double>(generated.sample_rate) / targetRate;
    const size_t outputCount = static_cast<size_t>(
        std::ceil(generated.samples.size() / ratio));
    std::vector<int16_t> pcm(outputCount);
    for (size_t i = 0; i < outputCount; ++i) {
        const double sourcePosition = i * ratio;
        const size_t left = std::min(static_cast<size_t>(sourcePosition),
                                     generated.samples.size() - 1);
        const size_t right = std::min(left + 1, generated.samples.size() - 1);
        const float fraction = static_cast<float>(sourcePosition - left);
        const float sample = generated.samples[left] * (1.0f - fraction) +
                             generated.samples[right] * fraction;
        const float clipped = std::max(-1.0f, std::min(1.0f, sample));
        pcm[i] = static_cast<int16_t>(clipped * 32767.0f);
    }

    size_t offset = 0;
    while (offset < pcm.size()) {
        const int32_t frames = static_cast<int32_t>(
            std::min<size_t>(2048, pcm.size() - offset));
        const aaudio_result_t written = AAudioStream_write(
            output, pcm.data() + offset, frames, 1000000000LL);
        if (written <= 0) {
            LOGE("Hindi speech output failed: %s", AAudio_convertResultToText(written));
            break;
        }
        offset += static_cast<size_t>(written);
    }

    AAudioStream_requestStop(output);
    AAudioStream_close(output);
}

void speak_hindi_async(Engine& e, const std::string& text) {
    if (text.empty()) return;
    std::lock_guard<std::mutex> launchLock(e.ttsMutex);
    if (e.shuttingDown.load() || e.speaking.load()) return;
    if (e.ttsThread.joinable() && e.ttsThread.get_id() != std::this_thread::get_id()) {
        e.ttsThread.join();
    }

    e.speaking.store(true);
    e.ttsThread = std::thread([&e, text] {
        const std::string baseDir = e.app->activity->internalDataPath;
        const std::string modelDir = baseDir + "/jarvis-hindi-voice";
        const std::string espeakDir = baseDir + "/espeak-ng-data";
        mkdir(modelDir.c_str(), 0700);

        const std::string modelPath = modelDir + "/model.onnx";
        const std::string tokensPath = modelDir + "/tokens.txt";
        const bool ready =
            copy_asset_to_file(e.app->activity->assetManager,
                               "jarvis-tts/model.onnx", modelPath) &&
            copy_asset_to_file(e.app->activity->assetManager,
                               "jarvis-tts/tokens.txt", tokensPath) &&
            copy_asset_to_file(e.app->activity->assetManager,
                               "jarvis-tts/model.onnx.json", modelPath + ".json") &&
            copy_asset_tree(e.app->activity->assetManager,
                            "espeak-ng-data", espeakDir);

        if (!ready) {
            set_status(e, "HINDI VOICE ASSETS NOT READY");
            e.speaking.store(false);
            e.restartWake.store(true);
            return;
        }

        if (piper_phonemize::cxx::Initialize(espeakDir) < 0) {
            set_status(e, "HINDI PHONEMIZER INITIALIZATION FAILED");
            e.speaking.store(false);
            e.restartWake.store(true);
            return;
        }

        if (!e.hindiTts || !e.hindiTts->Get()) {
            set_status(e, "LOADING HINDI VOICE MODEL");
            sherpa_onnx::cxx::OfflineTtsConfig config;
            config.model.vits.model = modelPath;
            config.model.vits.tokens = tokensPath;
            config.model.vits.noise_scale = 0.667f;
            config.model.vits.noise_scale_w = 0.8f;
            config.model.vits.length_scale = 1.0f;
            config.model.num_threads = 2;
            config.model.provider = "cpu";
            config.model.debug = false;

            auto model = sherpa_onnx::cxx::OfflineTts::Create(config);
            if (!model.Get()) {
                set_status(e, "HINDI VOICE MODEL FAILED TO LOAD");
                e.speaking.store(false);
                e.restartWake.store(true);
                return;
            }
            e.hindiTts = std::make_unique<sherpa_onnx::cxx::OfflineTts>(std::move(model));
        }

        const std::string voice = e.hindiTts->Lang().empty() ? "hi" : e.hindiTts->Lang();
        const auto phonemes = piper_phonemize::cxx::Phonemize(text, voice);
        if (phonemes.sentences.empty()) {
            set_status(e, "HINDI TEXT PHONEMIZATION FAILED");
            e.speaking.store(false);
            e.restartWake.store(true);
            return;
        }

        sherpa_onnx::cxx::GenerationConfig generation;
        generation.sid = 0;
        generation.speed = 1.0f;
        generation.phoneme_codepoints = phonemes.sentences;

        set_status(e, "SPEAKING HINDI RESPONSE");
        const auto audio = e.hindiTts->Generate("", generation);
        if (audio.samples.empty()) {
            set_status(e, "HINDI SPEECH GENERATION FAILED");
        } else {
            play_generated_audio(audio);
        }

        e.speaking.store(false);
        e.restartWake.store(true);
    });
}


std::string local_llm_model_path(Engine& e) {
    return std::string(e.app->activity->internalDataPath) + "/Qwen3-0.6B.Q4_K_M.gguf";
}

bool download_model_file(Engine& e, const std::string& url, const std::string& path) {
    JavaVM* vm = e.app->activity->vm;
    JNIEnv* env = nullptr;
    if (!vm || vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
        set_status(e, "ANDROID NETWORK THREAD COULD NOT ATTACH");
        return false;
    }

    bool success = false;
    jobject connection = nullptr;
    jobject input = nullptr;
    jclass urlClass = env->FindClass("java/net/URL");
    jmethodID urlConstructor = urlClass ? env->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V") : nullptr;
    jstring urlText = env->NewStringUTF(url.c_str());
    jobject urlObject = (urlConstructor && urlText) ? env->NewObject(urlClass, urlConstructor, urlText) : nullptr;
    if (urlObject && !env->ExceptionCheck()) {
        jmethodID openConnection = env->GetMethodID(urlClass, "openConnection", "()Ljava/net/URLConnection;");
        connection = openConnection ? env->CallObjectMethod(urlObject, openConnection) : nullptr;
    }
    if (env->ExceptionCheck()) clear_jni_exception(env);

    jlong expectedBytes = -1;
    if (connection) {
        jclass connectionClass = env->GetObjectClass(connection);
        jmethodID setConnectTimeout = env->GetMethodID(connectionClass, "setConnectTimeout", "(I)V");
        jmethodID setReadTimeout = env->GetMethodID(connectionClass, "setReadTimeout", "(I)V");
        jmethodID getContentLength = env->GetMethodID(connectionClass, "getContentLengthLong", "()J");
        jmethodID getInputStream = env->GetMethodID(connectionClass, "getInputStream", "()Ljava/io/InputStream;");
        if (setConnectTimeout) env->CallVoidMethod(connection, setConnectTimeout, 30000);
        if (setReadTimeout) env->CallVoidMethod(connection, setReadTimeout, 45000);
        if (getContentLength) expectedBytes = env->CallLongMethod(connection, getContentLength);
        if (!env->ExceptionCheck() && getInputStream) input = env->CallObjectMethod(connection, getInputStream);
        if (env->ExceptionCheck()) clear_jni_exception(env);
        env->DeleteLocalRef(connectionClass);
    }

    if (input) {
        jclass inputClass = env->GetObjectClass(input);
        jmethodID readMethod = env->GetMethodID(inputClass, "read", "([BII)I");
        jmethodID closeMethod = env->GetMethodID(inputClass, "close", "()V");
        jbyteArray bytes = env->NewByteArray(65536);
        std::vector<jbyte> nativeBuffer(65536);
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        int64_t receivedBytes = 0;
        int lastProgress = -1;

        if (output && readMethod && bytes) {
            while (true) {
                const jint count = env->CallIntMethod(input, readMethod, bytes, 0, 65536);
                if (env->ExceptionCheck()) {
                    clear_jni_exception(env);
                    break;
                }
                if (count < 0) { success = true; break; }
                if (count == 0) continue;
                env->GetByteArrayRegion(bytes, 0, count, nativeBuffer.data());
                if (env->ExceptionCheck()) { clear_jni_exception(env); break; }
                output.write(reinterpret_cast<const char*>(nativeBuffer.data()), count);
                if (!output) break;
                receivedBytes += count;

                if (expectedBytes > 0) {
                    const int progress = static_cast<int>((receivedBytes * 100) / expectedBytes);
                    if (progress >= lastProgress + 5) {
                        set_status(e, "DOWNLOADING OFFLINE AI: " + std::to_string(std::min(progress, 100)) + "%");
                        lastProgress = progress;
                    }
                } else if (receivedBytes % (32LL * 1024LL * 1024LL) < count) {
                    set_status(e, "DOWNLOADING OFFLINE AI: " + std::to_string(receivedBytes / (1024LL * 1024LL)) + " MB");
                }
            }
            output.flush();
            success = success && output.good();
            if (expectedBytes > 0 && receivedBytes != expectedBytes) success = false;
        }

        if (closeMethod) env->CallVoidMethod(input, closeMethod);
        if (env->ExceptionCheck()) clear_jni_exception(env);
        if (bytes) env->DeleteLocalRef(bytes);
        env->DeleteLocalRef(inputClass);
    }

    if (input) env->DeleteLocalRef(input);
    if (connection) env->DeleteLocalRef(connection);
    if (urlObject) env->DeleteLocalRef(urlObject);
    if (urlText) env->DeleteLocalRef(urlText);
    if (urlClass) env->DeleteLocalRef(urlClass);
    vm->DetachCurrentThread();

    if (!success) std::remove(path.c_str());
    return success;
}

void start_offline_model_setup(Engine& e) {
    const std::string modelPath = local_llm_model_path(e);
    if (jarvis::offline_llm_model_exists(modelPath)) {
        e.localModelReady.store(true);
        set_status(e, "OFFLINE AI MODEL ALREADY INSTALLED");
        speak_hindi_async(e, "ऑफलाइन ए आई मॉडल पहले से तैयार है।");
        return;
    }

    if (e.modelDownloading.exchange(true)) {
        set_status(e, "OFFLINE AI DOWNLOAD ALREADY RUNNING");
        return;
    }
    if (e.modelDownloadThread.joinable()) e.modelDownloadThread.join();

    e.modelDownloadThread = std::thread([&e, modelPath] {
        set_status(e, "DOWNLOADING OFFLINE AI - 484 MB");
        const std::string url =
            "https://huggingface.co/QuantFactory/Qwen3-0.6B-GGUF/resolve/main/Qwen3-0.6B.Q4_K_M.gguf";
        const bool downloaded = download_model_file(e, url, modelPath);
        const bool valid = downloaded && jarvis::offline_llm_model_exists(modelPath);
        e.localModelReady.store(valid);
        e.modelDownloading.store(false);

        if (valid) {
            set_status(e, "OFFLINE QWEN AI MODEL INSTALLED");
            speak_hindi_async(e, "ऑफलाइन ए आई मॉडल डाउनलोड हो गया है। अब आप मुझसे सवाल पूछ सकते हैं।");
        } else {
            set_status(e, "OFFLINE AI DOWNLOAD FAILED - CHECK INTERNET");
            speak_hindi_async(e, "ऑफलाइन ए आई मॉडल डाउनलोड नहीं हो पाया। इंटरनेट कनेक्शन जाँचें।");
        }
    });
}

void start_offline_llm_reply(Engine& e, const std::string& question) {
    if (e.shuttingDown.load()) return;
    if (e.llmProcessing.exchange(true)) {
        set_status(e, "LOCAL AI IS ALREADY THINKING");
        return;
    }
    if (!jarvis::offline_llm_model_exists(local_llm_model_path(e))) {
        e.llmProcessing.store(false);
        set_status(e, "TAP AI SETUP TO DOWNLOAD OFFLINE MODEL");
        speak_hindi_async(e, "ऑफलाइन ए आई मॉडल अभी डाउनलोड नहीं हुआ है। पहले नीचे ए आई सेटअप दबाएँ।");
        return;
    }

    if (e.llmThread.joinable()) e.llmThread.join();
    e.llmThread = std::thread([&e, question] {
        set_status(e, "THINKING WITH LOCAL QWEN AI");
        std::string error;
        const std::string answer = jarvis::offline_llm_generate(
            local_llm_model_path(e), question, &error);
        if (answer.empty()) {
            set_status(e, error.empty() ? "LOCAL AI RETURNED NO ANSWER" : error);
            speak_hindi_async(e, "माफ़ कीजिए, अभी मैं इस सवाल का जवाब नहीं दे पाया।");
        } else {
            set_status(e, "OFFLINE AI RESPONSE READY");
            LOGI("Offline AI answer generated (%zu chars)", answer.size());
            speak_hindi_async(e, answer);
        }
        e.llmProcessing.store(false);
    });
}

void start_transcription(Engine& e) {
    if (e.processing.load()) {
        set_status(e, "ASR IS STILL PROCESSING");
        return;
    }

    if (e.asrThread.joinable()) e.asrThread.join();

    std::vector<float> samples;
    {
        std::lock_guard<std::mutex> lock(e.audioMutex);
        samples = e.recordedSamples;
    }

    if (samples.size() < 4000) {
        set_status(e, "TOO SHORT - SPEAK FOR 1 SECOND");
        return;
    }

    e.processing.store(true);
    set_status(e, "LOADING OFFLINE HINDI ASR");

    e.asrThread = std::thread([&e, samples = std::move(samples)]() mutable {
        const std::string modelDir = std::string(e.app->activity->internalDataPath) + "/jarvis-whisper-tiny";
        mkdir(modelDir.c_str(), 0700);

        const std::string encoderPath = modelDir + "/encoder.int8.onnx";
        const std::string decoderPath = modelDir + "/decoder.int8.onnx";
        const std::string tokensPath = modelDir + "/tokens.txt";

        bool ready = copy_asset_to_file(e.app->activity->assetManager,
                                        "sherpa-whisper-tiny/encoder.int8.onnx",
                                        encoderPath) &&
                     copy_asset_to_file(e.app->activity->assetManager,
                                        "sherpa-whisper-tiny/decoder.int8.onnx",
                                        decoderPath) &&
                     copy_asset_to_file(e.app->activity->assetManager,
                                        "sherpa-whisper-tiny/tokens.txt",
                                        tokensPath);
        if (!ready) {
            set_status(e, "ASR MODEL FILE SETUP FAILED");
            e.processing.store(false);
            e.restartWake.store(true);
            return;
        }

        set_status(e, "RUNNING OFFLINE SPEECH RECOGNITION");
        sherpa_onnx::cxx::OfflineRecognizerConfig config;
        config.feat_config.sample_rate = 16000;
        config.feat_config.feature_dim = 80;
        config.model_config.whisper.encoder = encoderPath;
        config.model_config.whisper.decoder = decoderPath;
        config.model_config.whisper.language = "";
        config.model_config.whisper.task = "transcribe";
        config.model_config.tokens = tokensPath;
        config.model_config.num_threads = 2;
        config.model_config.provider = "cpu";
        config.decoding_method = "greedy_search";

        auto recognizer = sherpa_onnx::cxx::OfflineRecognizer::Create(config);
        if (!recognizer.Get()) {
            set_status(e, "OFFLINE ASR MODEL FAILED TO LOAD");
            e.processing.store(false);
            e.restartWake.store(true);
            return;
        }

        auto stream = recognizer.CreateStream();
        stream.AcceptWaveform(e.capturedSampleRate, samples.data(),
                              static_cast<int32_t>(samples.size()));
        recognizer.Decode(&stream);
        const auto result = recognizer.GetResult(&stream);
        LOGI("Offline ASR transcript: %s", result.text.c_str());

        const std::string contactQuery = jarvis::extract_contact_query(result.text);
        const std::string youtubeQuery = jarvis::extract_youtube_query(result.text);
        const jarvis::Command command = !contactQuery.empty()
            ? jarvis::Command::OpenWhatsAppContact
            : (!youtubeQuery.empty() ? jarvis::Command::SearchYouTube : jarvis::classify_command(result.text));
        if (!contactQuery.empty()) e.pendingContact = contactQuery;
        if (!youtubeQuery.empty()) e.pendingYoutubeQuery = youtubeQuery;
        if (command == jarvis::Command::Unknown) {
            if (!result.text.empty()) {
                e.pendingQuestion = result.text;
                e.pendingLlmQuery.store(true, std::memory_order_release);
                set_status(e, "QUESTION HEARD - ROUTING TO LOCAL AI");
            } else {
                set_status(e, "NO SPEECH RECOGNIZED");
            }
        } else {
            e.pendingCommand.store(static_cast<int>(command));
            set_status(e, "VOICE COMMAND RECOGNIZED");
        }
        e.processing.store(false);
        e.restartWake.store(true);
    });
}

void start_audio(Engine& e) {
    if (e.audioStream || e.recording.load()) return;
    if (!has_audio_permission(e)) {
        e.micPermissionPending.store(true);
        request_audio_permission(e);
        set_status(e, "ALLOW MIC PERMISSION TO START JARVIS");
        return;
    }
    e.micPermissionPending.store(false);

    if (e.asrThread.joinable()) e.asrThread.join();

    {
        std::lock_guard<std::mutex> lock(e.audioMutex);
        e.recordedSamples.clear();
        e.recordedSamples.reserve(static_cast<size_t>(16000) * 12);
    }
    {
        std::lock_guard<std::mutex> lock(e.kwsMutex);
        e.kwsAudioQueue.clear();
    }

    e.wakeDetected.store(false);
    e.wakeListening.store(true);
    e.commandReady.store(false);
    e.speechStarted.store(false);
    e.silenceFrames.store(0);
    e.commandFrames.store(0);
    e.kwsFailed.store(false);

    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) {
        set_status(e, "AUDIO INPUT UNAVAILABLE");
        e.wakeListening.store(false);
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
        set_status(e, "FAILED TO OPEN MICROPHONE");
        e.audioStream = nullptr;
        e.wakeListening.store(false);
        return;
    }

    e.capturedSampleRate = AAudioStream_getSampleRate(e.audioStream);
    const aaudio_result_t startResult = AAudioStream_requestStart(e.audioStream);
    if (startResult != AAUDIO_OK) {
        stop_audio(e);
        set_status(e, "MICROPHONE START FAILED");
        return;
    }

    e.recording.store(true);
    e.kwsThread = std::thread(run_wake_word_engine, std::ref(e));
    set_status(e, "LISTENING FOR JARVIS");
    LOGI("Native AAudio wake-word listener started");
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
        set_status(e, std::string("OPENED ") + packageName);
        env->DeleteLocalRef(intent);
    } else {
        set_status(e, std::string("APP NOT INSTALLED: ") + packageName);
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
        set_status(e, up ? "MEDIA VOLUME INCREASED" : "MEDIA VOLUME DECREASED");
    } else {
        set_status(e, "VOLUME CONTROL UNAVAILABLE");
    }

    env->DeleteLocalRef(service);
    env->DeleteLocalRef(cls);
}

void launch_dialer(Engine& e) {
    JNIEnv* env = e.app->activity->env;
    jclass intentClass = env->FindClass("android/content/Intent");
    jmethodID constructor = env->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;)V");
    jstring action = env->NewStringUTF("android.intent.action.DIAL");
    jobject intent = env->NewObject(intentClass, constructor, action);
    jclass activityClass = env->GetObjectClass(e.app->activity->clazz);
    jmethodID startActivity = env->GetMethodID(
        activityClass, "startActivity", "(Landroid/content/Intent;)V");
    env->CallVoidMethod(e.app->activity->clazz, startActivity, intent);
    clear_jni_exception(env);
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(intent);
    env->DeleteLocalRef(action);
    env->DeleteLocalRef(intentClass);
}

struct ContactMatch { std::string name; std::string number; };

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
    if (!value) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) { clear_jni_exception(env); return {}; }
    std::string text(chars);
    env->ReleaseStringUTFChars(value, chars);
    return text;
}

std::string normalize_contact_number(const std::string& value) {
    std::string digits;
    for (unsigned char ch : value) if (std::isdigit(ch)) digits.push_back(static_cast<char>(ch));
    if (digits.size() == 11 && digits.front() == '0') digits.erase(digits.begin());
    if (digits.size() == 10) digits = "91" + digits;
    return digits;
}

std::string lower_contact_name(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return ch < 0x80 ? static_cast<char>(std::tolower(ch)) : static_cast<char>(ch); });
    return value;
}

bool launch_whatsapp_chat(Engine& e, const std::string& phone) {
    JNIEnv* env = e.app->activity->env;
    jclass uriClass = env->FindClass("android/net/Uri");
    jmethodID parse = env->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    const std::string url = "https://wa.me/" + phone + "?text=Hi";
    jstring urlValue = env->NewStringUTF(url.c_str());
    jobject uri = env->CallStaticObjectMethod(uriClass, parse, urlValue);
    clear_jni_exception(env);

    jclass intentClass = env->FindClass("android/content/Intent");
    jmethodID constructor = env->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;)V");
    jstring action = env->NewStringUTF("android.intent.action.VIEW");
    jobject intent = env->NewObject(intentClass, constructor, action);
    jmethodID setData = env->GetMethodID(intentClass, "setData", "(Landroid/net/Uri;)Landroid/content/Intent;");
    env->CallObjectMethod(intent, setData, uri);
    jclass activityClass = env->GetObjectClass(e.app->activity->clazz);
    jmethodID startActivity = env->GetMethodID(activityClass, "startActivity", "(Landroid/content/Intent;)V");
    env->CallVoidMethod(e.app->activity->clazz, startActivity, intent);
    const bool success = !env->ExceptionCheck();
    clear_jni_exception(env);

    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(intent);
    env->DeleteLocalRef(action);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(uri);
    env->DeleteLocalRef(urlValue);
    env->DeleteLocalRef(uriClass);
    return success;
}

void open_whatsapp_contact(Engine& e, const std::string& query) {
    if (query.empty()) {
        set_status(e, "CONTACT NAME NOT RECOGNIZED");
        speak_hindi_async(e, "कृपया संपर्क का नाम बताइए।");
        return;
    }
    if (!has_contacts_permission(e)) {
        set_status(e, "CONTACTS PERMISSION REQUIRED");
        request_contacts_permission(e);
        speak_hindi_async(e, "संपर्क खोजने के लिए कॉन्टैक्ट अनुमति दें, फिर नाम दोबारा बोलें।");
        return;
    }

    JNIEnv* env = e.app->activity->env;
    jobject activity = e.app->activity->clazz;
    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getResolver = env->GetMethodID(activityClass, "getContentResolver", "()Landroid/content/ContentResolver;");
    jobject resolver = env->CallObjectMethod(activity, getResolver);
    clear_jni_exception(env);
    if (!resolver) {
        set_status(e, "CONTACT PROVIDER UNAVAILABLE");
        env->DeleteLocalRef(activityClass);
        return;
    }

    jclass uriClass = env->FindClass("android/net/Uri");
    jmethodID parse = env->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jstring uriText = env->NewStringUTF("content://com.android.contacts/data/phones");
    jobject uri = env->CallStaticObjectMethod(uriClass, parse, uriText);
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray projection = env->NewObjectArray(2, stringClass, nullptr);
    jstring nameColumn = env->NewStringUTF("display_name");
    jstring numberColumn = env->NewStringUTF("data1");
    env->SetObjectArrayElement(projection, 0, nameColumn);
    env->SetObjectArrayElement(projection, 1, numberColumn);
    jclass resolverClass = env->GetObjectClass(resolver);
    jmethodID queryMethod = env->GetMethodID(resolverClass, "query", "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;");
    jobject cursor = env->CallObjectMethod(resolver, queryMethod, uri, projection, nullptr, nullptr, nullptr);
    clear_jni_exception(env);

    std::vector<ContactMatch> exact;
    std::vector<ContactMatch> partial;
    if (cursor) {
        jclass cursorClass = env->GetObjectClass(cursor);
        jmethodID getColumnIndex = env->GetMethodID(cursorClass, "getColumnIndex", "(Ljava/lang/String;)I");
        jmethodID moveToNext = env->GetMethodID(cursorClass, "moveToNext", "()Z");
        jmethodID getString = env->GetMethodID(cursorClass, "getString", "(I)Ljava/lang/String;");
        jmethodID close = env->GetMethodID(cursorClass, "close", "()V");
        const jint nameIndex = env->CallIntMethod(cursor, getColumnIndex, nameColumn);
        const jint numberIndex = env->CallIntMethod(cursor, getColumnIndex, numberColumn);
        const std::string needle = lower_contact_name(query);
        while (env->CallBooleanMethod(cursor, moveToNext)) {
            jstring nameValue = static_cast<jstring>(env->CallObjectMethod(cursor, getString, nameIndex));
            jstring numberValue = static_cast<jstring>(env->CallObjectMethod(cursor, getString, numberIndex));
            const std::string name = jstring_to_utf8(env, nameValue);
            const std::string number = jstring_to_utf8(env, numberValue);
            if (nameValue) env->DeleteLocalRef(nameValue);
            if (numberValue) env->DeleteLocalRef(numberValue);
            if (name.empty() || number.empty()) continue;
            const std::string candidate = lower_contact_name(name);
            if (candidate == needle) exact.push_back({name, number});
            else if (candidate.find(needle) != std::string::npos || needle.find(candidate) != std::string::npos) partial.push_back({name, number});
        }
        env->CallVoidMethod(cursor, close);
        env->DeleteLocalRef(cursorClass);
    }

    env->DeleteLocalRef(resolverClass);
    env->DeleteLocalRef(numberColumn);
    env->DeleteLocalRef(nameColumn);
    env->DeleteLocalRef(projection);
    env->DeleteLocalRef(stringClass);
    env->DeleteLocalRef(uri);
    env->DeleteLocalRef(uriText);
    env->DeleteLocalRef(uriClass);
    env->DeleteLocalRef(resolver);
    env->DeleteLocalRef(activityClass);
    clear_jni_exception(env);

    auto deduplicate = [](std::vector<ContactMatch>& items) {
        std::vector<ContactMatch> unique;
        for (const auto& item : items) {
            const std::string phone = normalize_contact_number(item.number);
            const bool already = std::any_of(unique.begin(), unique.end(), [&](const ContactMatch& saved) {
                return normalize_contact_number(saved.number) == phone;
            });
            if (!already) unique.push_back(item);
        }
        items.swap(unique);
    };
    deduplicate(exact);
    deduplicate(partial);
    std::vector<ContactMatch>& matches = exact.empty() ? partial : exact;
    if (matches.empty()) {
        set_status(e, "WHATSAPP CONTACT NOT FOUND");
        speak_hindi_async(e, "यह संपर्क नहीं मिला। कृपया नाम दोबारा बोलें।");
        return;
    }
    if (matches.size() > 1) {
        set_status(e, "MULTIPLE CONTACTS - SAY FULL NAME");
        speak_hindi_async(e, "इस नाम से कई संपर्क मिले हैं। कृपया पूरा नाम बोलिए।");
        return;
    }
    const std::string phone = normalize_contact_number(matches.front().number);
    if (phone.size() < 10) {
        set_status(e, "INVALID CONTACT PHONE NUMBER");
        speak_hindi_async(e, "इस संपर्क का फोन नंबर सही नहीं है।");
        return;
    }
    if (launch_whatsapp_chat(e, phone)) {
        set_status(e, "WHATSAPP CHAT OPENED - HI PREFILLED");
        speak_hindi_async(e, matches.front().name + " की व्हाट्सऐप चैट खुल गई है। संदेश भेजने के लिए भेजें दबाएँ।");
    } else {
        set_status(e, "WHATSAPP CHAT COULD NOT OPEN");
        speak_hindi_async(e, "व्हाट्सऐप चैट नहीं खुल सकी।");
    }
}

std::string encode_url_query(const std::string& value) {
    static const char* hex = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char ch : value) {
        const bool safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                          (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
                          ch == '.' || ch == '~';
        if (safe) encoded.push_back(static_cast<char>(ch));
        else if (ch == ' ') encoded.push_back('+');
        else {
            encoded.push_back('%');
            encoded.push_back(hex[(ch >> 4) & 0x0F]);
            encoded.push_back(hex[ch & 0x0F]);
        }
    }
    return encoded;
}

bool open_external_url(Engine& e, const std::string& url, const char* packageName = nullptr) {
    JNIEnv* env = e.app->activity->env;
    jclass uriClass = env->FindClass("android/net/Uri");
    jmethodID parse = env->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jstring urlText = env->NewStringUTF(url.c_str());
    jobject uri = env->CallStaticObjectMethod(uriClass, parse, urlText);
    jclass intentClass = env->FindClass("android/content/Intent");
    jmethodID constructor = env->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;)V");
    jstring action = env->NewStringUTF("android.intent.action.VIEW");
    jobject intent = env->NewObject(intentClass, constructor, action);
    jmethodID setData = env->GetMethodID(intentClass, "setData", "(Landroid/net/Uri;)Landroid/content/Intent;");
    env->CallObjectMethod(intent, setData, uri);
    if (packageName) {
        jmethodID setPackage = env->GetMethodID(intentClass, "setPackage", "(Ljava/lang/String;)Landroid/content/Intent;");
        jstring package = env->NewStringUTF(packageName);
        env->CallObjectMethod(intent, setPackage, package);
        env->DeleteLocalRef(package);
    }
    jclass activityClass = env->GetObjectClass(e.app->activity->clazz);
    jmethodID startActivity = env->GetMethodID(activityClass, "startActivity", "(Landroid/content/Intent;)V");
    env->CallVoidMethod(e.app->activity->clazz, startActivity, intent);
    const bool success = !env->ExceptionCheck();
    clear_jni_exception(env);
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(intent);
    env->DeleteLocalRef(action);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(uri);
    env->DeleteLocalRef(urlText);
    env->DeleteLocalRef(uriClass);
    return success;
}

void execute_command(Engine& e, jarvis::Command command, const std::string& contactQuery = {}, const std::string& youtubeQuery = {}) {
    switch (command) {
        case jarvis::Command::OpenWhatsAppContact:
            open_whatsapp_contact(e, contactQuery);
            break;
        case jarvis::Command::OpenWhatsApp:
            launch_package(e, "com.whatsapp");
            speak_hindi_async(e, "व्हाट्सऐप खोल रहा हूँ।");
            break;
        case jarvis::Command::OpenYouTube:
            launch_package(e, "com.google.android.youtube");
            speak_hindi_async(e, "यूट्यूब खोल रहा हूँ।");
            break;
        case jarvis::Command::SearchYouTube: {
            const std::string query = youtubeQuery.empty() ? "trending videos" : youtubeQuery;
            const std::string url = "https://www.youtube.com/results?search_query=" + encode_url_query(query);
            if (!open_external_url(e, url, "com.google.android.youtube")) {
                if (!open_external_url(e, url)) launch_package(e, "com.google.android.youtube");
            }
            set_status(e, "YOUTUBE SEARCH: " + query);
            speak_hindi_async(e, "यूट्यूब पर " + query + " खोज रहा हूँ।");
            break;
        }
        case jarvis::Command::VolumeUp:
            change_volume(e, true);
            speak_hindi_async(e, "आवाज़ बढ़ा दी है।");
            break;
        case jarvis::Command::VolumeDown:
            change_volume(e, false);
            speak_hindi_async(e, "आवाज़ कम कर दी है।");
            break;
        case jarvis::Command::OpenDialer:
            launch_dialer(e);
            set_status(e, "DIALER OPENED - CALL NOT PLACED");
            speak_hindi_async(e, "फोन डायलर खोल दिया है। कॉल करने से पहले नंबर जाँच लें।");
            break;
        default:
            set_status(e, "COMMAND NOT RECOGNIZED");
            speak_hindi_async(e, "माफ़ कीजिए, मैं आपका आदेश समझ नहीं पाया।");
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
        if (e->speaking.load()) {
            set_status(*e, "JARVIS IS SPEAKING");
            return 1;
        }
        if (e->processing.load()) {
            set_status(*e, "OFFLINE ASR IS PROCESSING");
            return 1;
        }
        if (e->recording.load()) {
            const bool commandWasTriggered = e->wakeDetected.load();
            stop_audio(*e);
            if (commandWasTriggered) {
                set_status(*e, "MIC LISTENING STOPPED - TRANSCRIBING");
                start_transcription(*e);
            } else {
                set_status(*e, "WAKE WORD LISTENING STOPPED");
            }
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
        start_offline_model_setup(*e);
        return 1;
    }
    return 1;
}

void handle_cmd(android_app* app, int32_t cmd) {
    auto* e = static_cast<Engine*>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window && init_gl(*e)) draw_ui(*e);
            else set_status(*e, "OPENGL INITIALIZATION FAILED");
            break;
        case APP_CMD_TERM_WINDOW:
            shutdown_gl(*e);
            break;
        case APP_CMD_GAINED_FOCUS:
            e->autoWakePending.store(true);
            if (e->display != EGL_NO_DISPLAY) draw_ui(*e);
            break;
        case APP_CMD_WINDOW_RESIZED:
            if (e->display != EGL_NO_DISPLAY) draw_ui(*e);
            break;
        case APP_CMD_LOST_FOCUS:
            if (e->recording.load()) {
                stop_audio(*e);
                set_status(*e, "WAKE LISTENER PAUSED - APP NOT IN FOREGROUND");
            }
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
                engine.shuttingDown.store(true);
                stop_audio(engine);
                if (engine.asrThread.joinable()) engine.asrThread.join();
                if (engine.llmThread.joinable()) engine.llmThread.join();
                if (engine.modelDownloadThread.joinable()) engine.modelDownloadThread.join();
                if (engine.ttsThread.joinable()) engine.ttsThread.join();
                jarvis::offline_llm_shutdown();
                shutdown_gl(engine);
                return;
            }
        } while (ident >= 0);
        if (engine.kwsFailed.exchange(false)) {
            stop_audio(engine);
        }

        if (engine.autoWakePending.exchange(false) && !engine.recording.load() &&
            !engine.processing.load() && !engine.speaking.load() && !engine.modelDownloading.load()) {
            start_audio(engine);
        }

        if (engine.micPermissionPending.load() && has_audio_permission(engine) &&
            !engine.recording.load() && !engine.processing.load()) {
            engine.micPermissionPending.store(false);
            start_audio(engine);
        }

        if (engine.commandReady.exchange(false) && engine.recording.load()) {
            stop_audio(engine);
            set_status(engine, "COMMAND CAPTURED - TRANSCRIBING OFFLINE");
            start_transcription(engine);
        }

        if (engine.pendingLlmQuery.exchange(false)) {
            const std::string question = engine.pendingQuestion;
            engine.pendingQuestion.clear();
            start_offline_llm_reply(engine, question);
        }

        const int completedCommand = engine.pendingCommand.exchange(0);
        if (completedCommand > 0) {
            const std::string contact = std::move(engine.pendingContact);
            const std::string youtube = std::move(engine.pendingYoutubeQuery);
            engine.pendingContact.clear();
            engine.pendingYoutubeQuery.clear();
            execute_command(engine, static_cast<jarvis::Command>(completedCommand), contact, youtube);
        }

        if (engine.restartWake.exchange(false) && !engine.processing.load() &&
            !engine.llmProcessing.load() && !engine.speaking.load() &&
            !engine.recording.load() && !engine.shuttingDown.load() &&
            !app->destroyRequested) {
            start_audio(engine);
        }

        if (engine.display != EGL_NO_DISPLAY) draw_ui(engine);
    }
}
