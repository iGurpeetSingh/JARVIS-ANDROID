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

    if (contains_any(text, {"volume up", "increase volume", "turn up volume", "volume badhao",
                            "awaz badhao", "आवाज बढ़", "वॉल्यूम बढ़", "आवाज़ बढ़",
                            "आवाज तेज", "आवाज़ तेज", "आवाज़ ऊँची"})) {
        return Command::VolumeUp;
    }
    if (contains_any(text, {"volume down", "decrease volume", "turn down volume", "volume kam karo",
                            "awaz kam", "आवाज कम", "वॉल्यूम कम", "आवाज़ कम",
                            "आवाज धीमी", "आवाज़ धीमी"})) {
        return Command::VolumeDown;
    }
    if (contains_any(text, {"whatsapp", "व्हाट्सएप", "व्हाट्स ऐप", "व्हाट्सएप खोल",
                            "व्हाट्सऐप खोल", "whatsapp kholo", "whatsapp open"})) {
        return Command::OpenWhatsApp;
    }
    if (contains_any(text, {"youtube", "यूट्यूब", "youtube kholo", "youtube open", "यूट्यूब खोल"})) {
        return Command::OpenYouTube;
    }
    if (contains_any(text, {"call", "कॉल", "फोन लगाओ", "डायलर खोल", "phone kholo",
                            "open dialer", "dialer kholo"})) {
        return Command::OpenDialer;
    }
    return Command::Unknown;
}
}
