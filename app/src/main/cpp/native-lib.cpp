#include <jni.h>
#include <string>
#include <algorithm>
#include <cctype>

namespace {

std::string to_ascii_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       // Preserve UTF-8 bytes for Hindi text; lowercase ASCII only.
                       return ch < 0x80 ? static_cast<char>(std::tolower(ch))
                                        : static_cast<char>(ch);
                   });
    return value;
}

bool contains_any(const std::string& text,
                  const std::initializer_list<const char*>& phrases) {
    for (const char* phrase : phrases) {
        if (text.find(phrase) != std::string::npos) return true;
    }
    return false;
}

// Command IDs are mirrored in NativeCommandEngine.kt.
int classify(const std::string& raw) {
    const std::string text = to_ascii_lower(raw);

    if (contains_any(text, {"volume up", "increase volume", "आवाज बढ़",
                            "वॉल्यूम बढ़", "आवाज़ बढ़"})) return 1;
    if (contains_any(text, {"volume down", "decrease volume", "आवाज कम",
                            "वॉल्यूम कम", "आवाज़ कम"})) return 2;
    if (contains_any(text, {"whatsapp", "व्हाट्सएप", "व्हाट्स ऐप"})) return 3;
    if (contains_any(text, {"youtube", "यूट्यूब"})) return 4;
    if (contains_any(text, {"call", "कॉल", "फोन लगाओ", "डायलर खोल"})) return 5;

    return 0;
}

}  // namespace

extern "C"
JNIEXPORT jint JNICALL
Java_com_jarvis_assistant_NativeCommandEngine_classifyCommand(
        JNIEnv* env, jobject /* instance */, jstring command) {
    if (command == nullptr) return 0;

    const char* chars = env->GetStringUTFChars(command, nullptr);
    if (chars == nullptr) return 0;

    const int result = classify(chars);
    env->ReleaseStringUTFChars(command, chars);
    return result;
}
