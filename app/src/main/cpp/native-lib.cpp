#include "command_engine.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>

namespace {
std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
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
}

namespace jarvis {
Command classify_command(const std::string& raw) {
    const std::string text = lower_ascii(raw);

    if (contains_any(text, {"volume up", "increase volume", "आवाज बढ़",
                            "वॉल्यूम बढ़", "आवाज़ बढ़"})) {
        return Command::VolumeUp;
    }
    if (contains_any(text, {"volume down", "decrease volume", "आवाज कम",
                            "वॉल्यूम कम", "आवाज़ कम"})) {
        return Command::VolumeDown;
    }
    if (contains_any(text, {"whatsapp", "व्हाट्सएप", "व्हाट्स ऐप"})) {
        return Command::OpenWhatsApp;
    }
    if (contains_any(text, {"youtube", "यूट्यूब"})) {
        return Command::OpenYouTube;
    }
    if (contains_any(text, {"call", "कॉल", "फोन लगाओ", "डायलर खोल"})) {
        return Command::OpenDialer;
    }
    return Command::Unknown;
}
}
